----------------------------------------------------------------
--  MISCELLANEOUS PREFABS
----------------------------------------------------------------
--
--  Oblige Level Maker
--
--  Copyright (C) 2010 Andrew Apted
--
--  This program is free software; you can redistribute it and/or
--  modify it under the terms of the GNU General Public License
--  as published by the Free Software Foundation; either version 2
--  of the License, or (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU General Public License for more details.
--
----------------------------------------------------------------

PREFAB.PILLAR =
{
  brushes=
  {
    -- main stem
    {
      { x =  32, y = -32, mat = "?pillar", peg=1, x_offset=0, y_offset=0 },
      { x =  32, y =  32, mat = "?pillar", peg=1, x_offset=0, y_offset=0 },
      { x = -32, y =  32, mat = "?pillar", peg=1, x_offset=0, y_offset=0 },
      { x = -32, y = -32, mat = "?pillar", peg=1, x_offset=0, y_offset=0 },
    },

    -- trim closest to stem
    {
      { x =  40, y = -40, mat = "?trim2", blocked=1 },
      { x =  40, y =  40, mat = "?trim2", blocked=1 },
      { x = -40, y =  40, mat = "?trim2", blocked=1 },
      { x = -40, y = -40, mat = "?trim2", blocked=1 },
      { t = 20, mat = "?trim2" },
    },

    {
      { x =  40, y = -40, mat = "?trim2", blocked=1 },
      { x =  40, y =  40, mat = "?trim2", blocked=1 },
      { x = -40, y =  40, mat = "?trim2", blocked=1 },
      { x = -40, y = -40, mat = "?trim2", blocked=1 },
      { b = 108, mat = "?trim2" },
    },

    -- roundish and lowest trim
    {
      { x = -40, y = -56, mat = "?trim1" },
      { x =  40, y = -56, mat = "?trim1" },
      { x =  56, y = -40, mat = "?trim1" },
      { x =  56, y =  40, mat = "?trim1" },
      { x =  40, y =  56, mat = "?trim1" },
      { x = -40, y =  56, mat = "?trim1" },
      { x = -56, y =  40, mat = "?trim1" },
      { x = -56, y = -40, mat = "?trim1" },
      { t = 6, mat = "?trim1" },
    },

    {
      { x = -40, y = -56, mat = "?trim1" },
      { x =  40, y = -56, mat = "?trim1" },
      { x =  56, y = -40, mat = "?trim1" },
      { x =  56, y =  40, mat = "?trim1" },
      { x =  40, y =  56, mat = "?trim1" },
      { x = -40, y =  56, mat = "?trim1" },
      { x = -56, y =  40, mat = "?trim1" },
      { x = -56, y = -40, mat = "?trim1" },
      { b = 122, mat = "?trim1" },
    },
  },
}


PREFAB.CRATE =
{
  brushes =
  {
    {
      { x = -32, y = -32, mat = "?crate", peg=1, x_offset="?x_offset", y_offset="?y_offset" },
      { x =  32, y = -32, mat = "?crate", peg=1, x_offset="?x_offset", y_offset="?y_offset" },
      { x =  32, y =  32, mat = "?crate", peg=1, x_offset="?x_offset", y_offset="?y_offset" },
      { x = -32, y =  32, mat = "?crate", peg=1, x_offset="?x_offset", y_offset="?y_offset" },
      { t = 64, mat = "?crate", light = "?light" },
    },
  },
}

