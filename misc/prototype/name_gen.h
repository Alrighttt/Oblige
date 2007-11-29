//------------------------------------------------------------------------
//  READABLE NAME Generation
//------------------------------------------------------------------------
//
//  Oblige Level Maker (C) 2005 Andrew Apted
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#ifndef __OBLIGE_NAMEGEN_H__
#define __OBLIGE_NAMEGEN_H__


namespace name_gen
{
	const char *Encode(uint32_g seed);

	uint32_g Decode(const char *name);

	void JumpTest();
}


#endif /* __OBLIGE_NAMEGEN_H__ */
