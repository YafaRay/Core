#include <yafraycore/nodematerial.h>
#include <core_api/environment.h>
#include <set>

__BEGIN_YAFRAY

class YAFRAYCORE_EXPORT sNodeFinder_t: public nodeFinder_t
{
	public:
		sNodeFinder_t(const std::map<std::string,shaderNode_t *> &table): node_table(table) {}
		virtual const shaderNode_t* operator()(const std::string &name) const;
		
		virtual ~sNodeFinder_t(){};
	protected:
		const std::map<std::string,shaderNode_t *> &node_table;
};

const shaderNode_t* sNodeFinder_t::operator()(const std::string &name) const
{
	std::map<std::string,shaderNode_t *>::const_iterator i=node_table.find(name);
	if(i!=node_table.end()) return i->second;
	else return NULL;
}

void recursiveSolver(shaderNode_t *node, std::vector<shaderNode_t*> &sorted)
{
	if(node->ID != 0) return;
	node->ID=1;
	std::vector<const shaderNode_t*> deps;
	if(node->getDependencies(deps))
	{
		for(std::vector<const shaderNode_t*>::iterator i=deps.begin(); i!=deps.end(); ++i)
			// someone tell me a smarter way than casting away a const...
			if( (*i)->ID==0 ) recursiveSolver((shaderNode_t *)*i, sorted);
	}
	sorted.push_back(node);
}

void recursiveFinder(const shaderNode_t *node, std::set<const shaderNode_t*> &tree)
{
	std::vector<const shaderNode_t*> deps;
	if(node->getDependencies(deps))
	{
		for(std::vector<const shaderNode_t*>::iterator i=deps.begin(); i!=deps.end(); ++i)
		{
			tree.insert(*i);
			recursiveFinder(*i, tree);
		}
	}
	tree.insert(node);
}

nodeMaterial_t::~nodeMaterial_t()
{
	//clear nodes map:
	for(std::map<std::string,shaderNode_t *>::iterator i=mShadersTable.begin(); i!=mShadersTable.end(); ++i) delete i->second;
	mShadersTable.clear();
}

void nodeMaterial_t::solveNodesOrder(const std::vector<shaderNode_t *> &roots)
{
	//set all IDs = 0 to indicate "not tested yet"
	for(unsigned int i=0; i<allNodes.size(); ++i) allNodes[i]->ID=0;
	for(unsigned int i=0; i<roots.size(); ++i) recursiveSolver(roots[i], allSorted);
	if(allNodes.size() != allSorted.size()) Y_WARNING << "NodeMaterial: Unreachable nodes!" << yendl;
	//give the nodes an index to be used as the "stack"-index. 
	//using the order of evaluation can't hurt, can it?
	for(unsigned int i=0; i<allSorted.size(); ++i)
	{
		shaderNode_t *n = allSorted[i];
		n->ID=i;
		// sort nodes in view depandant and view independant
		// not sure if this is a good idea...should not include bump-only nodes
		//if(n->isViewDependant()) allViewdep.push_back(n);
		//else allViewindep.push_back(n);
	}
	reqNodeMem = allSorted.size() * sizeof(nodeResult_t);
}

/*! get a list of all nodes that are in the tree given by root
	prerequisite: nodes have been successfully loaded and stored into allSorted
	since "solveNodesOrder" sorts allNodes, calling getNodeList afterwards gives
	a list in evaluation order. multiple calls are merged in "nodes" */

void nodeMaterial_t::getNodeList(const shaderNode_t *root, std::vector<shaderNode_t *> &nodes)
{
	std::set<const shaderNode_t *> inTree;
	for(unsigned int i=0; i<nodes.size(); ++i) inTree.insert(nodes[i]);
	recursiveFinder(root, inTree);
	std::set<const shaderNode_t *>::iterator send=inTree.end();
	std::vector<shaderNode_t *>::iterator i, end=allSorted.end();
	nodes.clear();
	for(i=allSorted.begin(); i!=end; ++i) if(inTree.find(*i)!=send) nodes.push_back(*i);
}

void nodeMaterial_t::filterNodes(const std::vector<shaderNode_t *> &input, std::vector<shaderNode_t *> &output, int flags)
{
	for(unsigned int i=0; i<input.size(); ++i)
	{
		shaderNode_t *n = input[i];
		bool vp = n->isViewDependant();
		if((vp && flags&VIEW_DEP) || (!vp && flags&VIEW_INDEP)) output.push_back(n);
	}
}

void nodeMaterial_t::evalBump(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp, const shaderNode_t *bumpS)const
{
	std::vector<shaderNode_t *>::const_iterator iter, end=bumpNodes.end();
	for(iter = bumpNodes.begin(); iter!=end; ++iter) (*iter)->evalDerivative(stack, state, sp);
	CFLOAT du, dv;
	bumpS->getDerivative(stack, du, dv);
	applyBump(sp, du, dv);
}

bool nodeMaterial_t::loadNodes(const std::list<paraMap_t> &paramsList, renderEnvironment_t &render)
{
	bool error=false;
	const std::string *type=0;
    const std::string *name=0;
    const std::string *element=0;

	std::list<paraMap_t>::const_iterator i=paramsList.begin();
	
	for(; i!=paramsList.end(); ++i)
	{
		if( i->getParam("element", element))
		{
			if(*element != "shader_node") continue;
		}
		else Y_WARNING << "NodeMaterial: No element type given; assuming shader node" << yendl;
		
		if(! i->getParam("name", name) )
		{
			Y_ERROR << "NodeMaterial: Name of shader node not specified!" << yendl;
			error = true;
			break;
		}
		
		if(mShadersTable.find(*name) != mShadersTable.end() )
		{
			Y_ERROR << "NodeMaterial: Multiple nodes with identically names!" << yendl;
			error = true;
			break;
		}
		
		if(! i->getParam("type", type) )
		{
			Y_ERROR << "NodeMaterial: Type of shader node not specified!" << yendl;
			error = true;
			break;
		}
		
		renderEnvironment_t::shader_factory_t *fac = render.getShaderNodeFactory(*type);
		shaderNode_t *shader=0;
		
		if(fac)
        {
            shader = fac(*i, render);
        }
		else
		{
			Y_ERROR << "NodeMaterial: Don't know how to create shader node of type '"<<*type<<"'!" << yendl;
			error = true;
			break;
		}
		
		if(shader)
		{
			mShadersTable[*name] = shader;
			allNodes.push_back(shader);
			Y_INFO << "NodeMaterial: Added ShaderNode '"<<*name<<"'! ("<<(void*)shader<<")" << yendl;
		}
		else
		{
			Y_ERROR << "NodeMaterial: No shader node was constructed by plugin '"<<*type<<"'!" << yendl;
			error = true;
			break;
		}
	}
	
	if(!error) //configure node inputs
	{
		sNodeFinder_t finder(mShadersTable);
		int n=0;
		for(i=paramsList.begin(); i!=paramsList.end(); ++i, ++n)
		{
			if( !allNodes[n]->configInputs(*i, finder) )
			{
				Y_ERROR << "NodeMaterial: Shader node configuration failed! (n="<<n<<")" << yendl;
				error=true; break;
			}
		}
	}
	
	if(error)
	{
		//clear nodes map:
		for(std::map<std::string,shaderNode_t *>::iterator i=mShadersTable.begin();i!=mShadersTable.end();++i) delete i->second;
		mShadersTable.clear();
	}
	
	return !error;
}

void nodeMaterial_t::parseNodes(const paraMap_t &params, std::vector<shaderNode_t *> &roots, std::map<std::string, shaderNode_t *> &nodeList)
{
    const std::string *name=0;
    std::map<std::string, shaderNode_t *>::iterator currentNode;

    for(currentNode = nodeList.begin(); currentNode != nodeList.end(); ++currentNode)
    {
        if(params.getParam(currentNode->first, name))
        {
            std::map<std::string,shaderNode_t *>::const_iterator i = mShadersTable.find(*name);
         
            if(i!=mShadersTable.end())
            {
                currentNode->second = i->second;
                roots.push_back(currentNode->second);
            }
            else Y_WARNING << "Shader node " << currentNode->first << " '" << *name << "' does not exist!" << yendl;
        }
    }
}

__END_YAFRAY
