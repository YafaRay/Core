
#ifndef Y_SHADERMAT_H
#define Y_SHADERMAT_H

#include <yafray_config.h>
#include <core_api/material.h>
#include <core_api/shader.h>

__BEGIN_YAFRAY

class renderEnvironment_t;

enum nodeType_e { VIEW_DEP=1, VIEW_INDEP=1<<1 };

class YAFRAYCORE_EXPORT nodeMaterial_t: public material_t
{
	public:
		nodeMaterial_t(): reqNodeMem(0){}
	protected:
		/*! load nodes from parameter map list */
		bool loadNodes(const std::list<paraMap_t> &paramsList, renderEnvironment_t &render);
        /** parse node shaders to fill nodeList */
        void parseNodes(const paraMap_t &params, std::vector<shaderNode_t *> &roots, std::map<std::string, shaderNode_t *> &nodeList);
		/* put nodes in evaluation order in "allSorted" given all root nodes;
		   sets reqNodeMem to the amount of memory the node stack requires for evaluation of all nodes */
		void solveNodesOrder(const std::vector<shaderNode_t *> &roots);
		void getNodeList(const shaderNode_t *root, std::vector<shaderNode_t *> &nodes);
		void evalNodes(const renderState_t &state, const surfacePoint_t &sp, const std::vector<shaderNode_t *> &nodes, nodeStack_t &stack)const
		{
			std::vector<shaderNode_t *>::const_iterator iter, end=nodes.end();
			for(iter = nodes.begin(); iter!=end; ++iter) (*iter)->eval(stack, state, sp);
		}
		void evalBump(nodeStack_t &stack, const renderState_t &state, const surfacePoint_t &sp, const shaderNode_t *bumpS)const;
		/*! filter out nodes with specific properties */
		void filterNodes(const std::vector<shaderNode_t *> &input, std::vector<shaderNode_t *> &output, int flags);
		virtual ~nodeMaterial_t();
		
		std::vector<shaderNode_t *> allNodes, allSorted, allViewdep, allViewindep, bumpNodes;
		std::map<std::string,shaderNode_t *> mShadersTable;
		size_t reqNodeMem;
};

__END_YAFRAY

#endif // Y_SHADERMAT_H
