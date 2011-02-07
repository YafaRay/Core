/****************************************************************************
 *      console_verbosity.h: Console verbosity control
 *      This is part of the yafray package
 *      Copyright (C) 2010 Rodrigo Placencia Vazquez
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2.1 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library; if not, write to the Free Software
 *      Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
 
#ifndef Y_CONSOLE_VERBOSITY_H
#define Y_CONSOLE_VERBOSITY_H

#include <iostream>

__BEGIN_YAFRAY

enum
{
	VL_MUTE = 0,
	VL_ERROR,
	VL_WARNING,
	VL_INFO
};

class YAFRAYCORE_EXPORT OutputLevel
{
	int mVerbLevel;
	int mMasterVerbLevel;

public:

	OutputLevel(): mVerbLevel(VL_INFO), mMasterVerbLevel(VL_INFO) {}
	
	OutputLevel &info()
	{
		mVerbLevel = VL_INFO;
		return *this;
	}

	OutputLevel &warning()
	{
		mVerbLevel = VL_WARNING;
		return *this;
	}

	OutputLevel &error()
	{
		mVerbLevel = VL_ERROR;
		return *this;
	}

	void setMasterVerbosity(int vlevel)
	{
		mMasterVerbLevel = std::max( (int)VL_MUTE , std::min( vlevel, (int)VL_INFO ) );
	}

	template <typename T>
	OutputLevel &operator << ( const T &obj )
	{
		if(mVerbLevel <= mMasterVerbLevel)
		{
			std::cout << obj;
		}
		return *this;
	}

	OutputLevel &operator << ( std::ostream& (obj)(std::ostream&) )
	{
		if(mVerbLevel <= mMasterVerbLevel)
		{
			std::cout << obj;
		}
		return *this;
	}
};

extern YAFRAYCORE_EXPORT OutputLevel yafout;

__END_YAFRAY

#endif
