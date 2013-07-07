----------------------------------------------------------------
--  HEXAGONAL DEATH-MATCH
----------------------------------------------------------------
--
--  Oblige Level Maker
--
--  Copyright (C) 2013 Andrew Apted
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

--[[ *** CLASS INFORMATION ***

class HEXAGON
{
    cx, cy   -- position in cell map

    kind : keyword   -- "free", "used"
                     -- "edge", "wall"

    content : keyword  -- "START", "WEAPON", "FLAG", ...

    neighbor[HDIR] : HEXAGON   -- neighboring cells
                               -- will be NIL at edge of map
                               -- HDIR is 1 .. 6

    mid_x, mid_y  -- coordinate of mid point

    vertex[HDIR] : { x=#, y=# }  -- leftmost vertex for each edge

    thread : THREAD

    is_branch   -- true if a thread branched off here

    used_dist   -- distance to nearest used cell
}


class THREAD
{
    id : number

    start : HEXAGON   -- starting place (in an existing "used" cell)

    target : HEXAGON  -- ending place (an existing "used" cell)
    target_dir : dir  -- direction OUT of that cell

    pos : HEXAGON  -- last cell 'converted' to this thread

    dir : HDIR  -- current direction

    cells : array(HEXAGON)  -- all cells in this thread

    history : array(HDIR)   -- directions to follow from start cell

    grow_prob : number

    limit : number  -- when this reached zero, make a room
}


class ROOM
{
    id : number

    cells : list(HEXAGON)

    flag_room : boolean
}


Directions:
        _______
       /   5   \
      /4       6\
     /           \
     \           /
      \1       3/
       \___2___/


----------------------------------------------------------------]]


-- two dimensional grid / map
--
-- rows with an _even_ Y value are offset to the right:
--
--      1   2   3   4
--    1   2   3   4
--      1   2   3   4
--    1   2   3   4

HEX_MAP = {}

-- these must be odd (for CTF mode)
HEX_W = 15
HEX_H = 49

HEX_MID_X = 0  -- computed later
HEX_MID_Y = 0  --


HEX_LEFT  = { 2, 3, 6, 1, 4, 5 }
HEX_RIGHT = { 4, 1, 2, 5, 6, 3 }
HEX_OPP   = { 6, 5, 4, 3, 2, 1 }
HEX_DIRS  = { 1, 4, 5, 6, 3, 2 }


CTF_MODE = true


HEXAGON_CLASS = {}

function HEXAGON_CLASS.new(cx, cy)
  local C =
  {
    cx = cx
    cy = cy
    kind = "free"
    neighbor = {}
    vertex = {}
  }
  table.set_class(C, HEXAGON_CLASS)
  return C
end


function HEXAGON_CLASS.tostr(C)
  return string.format("CELL[%d,%d]", C.cx, C.cy)
end


function HEXAGON_CLASS.is_active(C)
  if not C.thread then return false end

  return not C.thread.dead
end


function HEXAGON_CLASS.free_neighbors(C)
  local count = 0

  for dir = 1,6 do
    local N = C.neighbor[dir]

    if N and N.kind == "free" then
      count = count + 1
    end
  end

  return count
end


function HEXAGON_CLASS.is_leaf(C)
  local count = 0

  for dir = 1, 6 do
    local N = C.neighbor[dir]

    if N and N.kind == "used" then
      count = count + 1
    end
  end

  if CTF_MODE and C.cy == HEX_MID_Y then
    return (count == 0)
  end

  return (count <= 1)
end


function HEXAGON_CLASS.can_join(C, T)
  local hit_used = false

  for i = 1, 6 do
    local N = C.neighbor[i]

    -- a thread cannot join onto itself

    if N.kind == "used" and N.thread != T then
      hit_used = true
    end
  end

  return hit_used
end


function HEXAGON_CLASS.used_dist_from_neighbors(C)
  local dist

  for i = 1, 6 do
    local N = C.neighbor[i]

    if N and N.used_dist and
       (not dist or N.used_dist < dist)
    then
      dist = N.used_dist
    end
  end
  
  return dist
end


function HEXAGON_CLASS.touches_edge(C)
  for i = 1, 6 do
    local N = C.neighbor[i]

    if not N or N.kind == "edge" then
      return true
    end
  end
 
  return false
end


function HEXAGON_CLASS.to_brush(C)
  local brush = {}

  for i = 6, 1, -1 do
    local dir = HEX_DIRS[i]

    local coord =
    {
      x = C.vertex[dir].x
      y = C.vertex[dir].y
    }

    table.insert(brush, coord)
  end

  return brush
end


function HEXAGON_CLASS.build(C)
  
  local f_h = rand.irange(0,6) * 0
  local c_h = rand.irange(4,8) * 32


  if C.kind == "fedge" or C.kind == "fwall" then --- or C.kind == "free" then
    local w_brush = C:to_brush()

    local w_mat = "ASHWALL4"

    if C.kind == "free" then w_mat = "COMPSPAN" end

if C.room then w_mat = C.room.f_mat end

    Brush_set_mat(w_brush, w_mat, w_mat)

    brush_helper(w_brush)
  else
    local f_brush = C:to_brush()

--    local f_mat = rand.pick({ "GRAY7", "MFLR8_3", "MFLR8_4", "STARTAN3",
--                              "TEKGREN2", "BROWN1" })

if not C.room then C.room = { f_mat="COMPSPAN" } end

    assert(C.room)

    if C.kind == "free" then
      f_mat = "NUKAGE1"
      f_h   = -16

    elseif C.kind == "used" then
      f_mat = "COMPBLUE"
    else
      f_mat = "GRAY7"
    end

f_mat = C.room.f_mat
f_h   = 0

    Brush_add_top(f_brush, f_h)
    Brush_set_mat(f_brush, f_mat, f_mat)

    brush_helper(f_brush)


    local c_brush = C:to_brush()

    Brush_add_bottom(c_brush, 256)
    Brush_mark_sky(c_brush)

    brush_helper(c_brush)
  end


  if C.content == "START" then
    entity_helper("dm_player", C.mid_x, C.mid_y, f_h, {})

    if not LEVEL.has_p1_start then
      entity_helper("player1", C.mid_x, C.mid_y, f_h, {})
      LEVEL.has_p1_start = true
    end
  end


  if C.content == "FLAG" then
    local ent = sel(C.cy < HEX_MID_Y, "blue_torch", "red_torch")
    entity_helper(ent, C.mid_x, C.mid_y, f_h, {})
  end


  if C.content == "ENTITY" then
    entity_helper(C.entity, C.mid_x, C.mid_y, f_h, {})
  
  elseif C.thread and not C.trimmed and not C.content then
    entity_helper("potion", C.mid_x, C.mid_y, f_h, {})

  end
end


----------------------------------------------------------------

H_WIDTH  = 80 + 40
H_HEIGHT = 64 + 32


function Hex_middle_coord(cx, cy)
  local x = H_WIDTH  * (1 + (cx - 1) * 3 + (1 - (cy % 2)) * 1.5)
  local y = H_HEIGHT * cy

  return math.round(x), math.round(y)
end


function Hex_neighbor_pos(cx, cy, dir)
  if dir == 2 then return cx, cy - 2 end
  if dir == 5 then return cx, cy + 2 end

  if (cy % 2) == 0 then
    if dir == 1 then return cx, cy - 1 end
    if dir == 4 then return cx, cy + 1 end
    if dir == 3 then return cx + 1, cy - 1 end
    if dir == 6 then return cx + 1, cy + 1 end
  else
    if dir == 1 then return cx - 1, cy - 1 end
    if dir == 4 then return cx - 1, cy + 1 end
    if dir == 3 then return cx, cy - 1 end
    if dir == 6 then return cx, cy + 1 end
  end
end


function Hex_vertex_coord(C, dir)
  local x, y

  if dir == 1 then
    x = C.mid_x - H_WIDTH / 2
    y = C.mid_y - H_HEIGHT
  elseif dir == 2 then
    x = C.mid_x + H_WIDTH / 2
    y = C.mid_y - H_HEIGHT
  elseif dir == 3 then
    x = C.mid_x + H_WIDTH
    y = C.mid_y
  elseif dir == 4 then
    x = C.mid_x - H_WIDTH
    y = C.mid_y
  elseif dir == 5 then
    x = C.mid_x - H_WIDTH / 2
    y = C.mid_y + H_HEIGHT
  elseif dir == 6 then
    x = C.mid_x + H_WIDTH / 2
    y = C.mid_y + H_HEIGHT
  end

  return math.round(x), math.round(y)
end


function Hex_setup()
  HEX_MAP = table.array_2D(HEX_W, HEX_H)

  HEX_MID_X = int((HEX_W + 1) / 2)
  HEX_MID_Y = int((HEX_H + 1) / 2)

  -- 1. create the hexagon cells

  for cx = 1, HEX_W do
  for cy = 1, HEX_H do
    local C = HEXAGON_CLASS.new(cx, cy)

    C.mid_x, C.mid_y = Hex_middle_coord(cx, cy)

    HEX_MAP[cx][cy] = C
  end
  end

  -- 2. setup neighbor links

  for cx = 1, HEX_W do
  for cy = 1, HEX_H do
    local C = HEX_MAP[cx][cy]

    local far_W = HEX_W - sel(CTF_MODE, (cy % 2), 0)

    for dir = 1,6 do
      local nx, ny = Hex_neighbor_pos(cx, cy, dir)

      if (nx >= 1) and (nx <= far_W) and
         (ny >= 1) and (ny <= HEX_H)
      then
        C.neighbor[dir] = HEX_MAP[nx][ny]
      else
        C.kind = "edge"
      end
    end
  end
  end

  -- 3. setup vertices

  for cx = 1, HEX_W do
  for cy = 1, HEX_H do
    local C = HEX_MAP[cx][cy]
  
    for dir = 1,6 do
      local x, y = Hex_vertex_coord(C, dir)

      C.vertex[dir] = { x=x, y=y }
    end
  end
  end

  -- 4. reset other stuff

  LEVEL.areas = {}
  LEVEL.rooms = {}

  collectgarbage()
end


function Hex_starting_area()
  LEVEL.start_cx = HEX_MID_X
  LEVEL.start_cy = HEX_MID_Y

  local C = HEX_MAP[LEVEL.start_cx][LEVEL.start_cy]

  C.kind = "used"
  C.content = "START"


  if CTF_MODE then
    local cx1 = HEX_MID_X - int(HEX_W / 4)
    local cx2 = HEX_MID_X + int(HEX_W / 4)

    if rand.odds(80) then
      -- sometimes remove middle
      if rand.odds(30) then
        C.kind = "free"
        C.content = nil
      end

      C = HEX_MAP[cx1][HEX_MID_Y]
      C.kind = "used"
--      C.content = "ENTITY"
--      C.entity  = "potion"

      C = HEX_MAP[cx2][HEX_MID_Y]
      C.kind = "used"
--      C.content = "ENTITY"
--      C.entity  = "potion"
    end
  end
end


function Hex_make_cycles()

  local threads = {}

  local MAX_THREAD = 30
  local total_thread = 0


  local function pick_dir(C)
    local dir_list = {}

    for dir = 1, 6 do
      local N = C.neighbor[dir]

      if CTF_MODE and dir >= 4 then continue end

      if N and N.kind == "free" and N:free_neighbors() == 5 then
        table.insert(dir_list, dir)
      end
    end

    if #dir_list == 0 then
      return nil
    end

    return rand.pick(dir_list)
  end


  local function pick_start()
    local list = {}

    -- collect all possible starting cells

    for cx = 1, HEX_W do
    for cy = 1, sel(CTF_MODE, HEX_MID_Y, HEX_H) do
      local C = HEX_MAP[cx][cy]

      if C.no_start then continue end

      if not (C.kind == "used" and not C:is_active()) then
        continue
      end

      if C:free_neighbors() < 3 then continue end

      table.insert(list, C)
    end
    end

    while #list > 0 do
      local idx = rand.irange(1, #list)

      local C = table.remove(list, idx)

      local dir = pick_dir(C)

      if dir then
        return C, dir  -- success
      end

      -- never try this cell again
      C.no_start = true
    end

    return nil  -- fail
  end


  local function do_grow_thread(T, dir, N)
    N.kind = "used"
    N.thread = T

    T.pos = N
    T.dir = dir

    table.insert(T.cells, N)
    table.insert(T.history, dir)
  end


  local function new_thread(start)
    return
    {
      id = Plan_alloc_id("hex_thread")

      start = start

      cells   = { }
      history = { }

      grow_dirs = rand.sel(50, { 2,3,4 }, { 4,3,2 })
      grow_prob = rand.pick({ 40, 60, 80 })

      limit = rand.irange(16, 48)
    }
  end


  local function add_thread()
    -- reached thread limit ?
    if total_thread >= MAX_THREAD then return end


    local start, dir = pick_start()

    if not start then return end

    local C1 = start.neighbor[dir]

    C1.is_branch = true


    local THREAD = new_thread(start)

    table.insert(threads, THREAD)

    do_grow_thread(THREAD, dir, C1)

    total_thread = total_thread + 1
  end


  local function respawn_thread(T)
    -- create a new thread which continues on where T left off

    local THREAD = new_thread(T.pos)

    THREAD.pos = T.pos
    THREAD.dir = T.dir

    table.insert(threads, THREAD)

    THREAD.pos.is_branch = true

    -- less quota for this thread
    total_thread = total_thread + 0.4

    return true
  end


  local function try_grow_thread_in_dir(T, dir)
    local N = T.pos.neighbor[dir]
    assert(N)

    if N.kind != "free" then return false end

    if CTF_MODE and N.cy >= HEX_MID_Y then return false end

    if N:free_neighbors() == 5 then
      do_grow_thread(T, dir, N)
      return true
    end

    if #T.history > 7 and N:can_join(T) then
      do_grow_thread(T, dir, N)

      T.target = N.neighbor[dir]
      T.target_dir = dir

      T.dead = true
      return true
    end

    return false
  end


  local function grow_a_thread(T)
    if T.limit <= 0 then
      T.dead = true

      -- debug crud...
      T.pos.content = "ENTITY"
      T.pos.entity  = "evil_eye"

      -- continue sometimes...
      if rand.odds(25) then
        respawn_thread(T)
      end

      return
    end

    T.limit = T.limit - 1


    local dir_L = HEX_LEFT [T.dir]
    local dir_R = HEX_RIGHT[T.dir]

    local check_dirs = {}
    
    check_dirs[dir_L] = T.grow_dirs[1]
    check_dirs[T.dir] = T.grow_dirs[2]
    check_dirs[dir_R] = T.grow_dirs[3]

    local tc = #T.history

    -- prevent too many steps in the same direction
    if tc >= 2 and T.history[tc] == T.history[tc - 1] then
      local d = T.history[tc]
      assert(check_dirs[d])

      if tc >= 3 and T.history[tc] == T.history[tc - 2] then
        check_dirs[d] = nil
      else
        check_dirs[d] = check_dirs[d] / 3
      end
    end

    while not table.empty(check_dirs) do
      local dir = rand.key_by_probs(check_dirs)
      check_dirs[dir] = nil

      if try_grow_thread_in_dir(T, dir) then
        return -- OK
      end
    end

    -- no direction was possible

    T.dead = true
  end


  local function grow_threads()
    for index = #threads, 1, -1 do
      
      local T = threads[index]

      if rand.odds(T.grow_prob) then
        grow_a_thread(T)

        if T.dead then
          table.remove(threads, index)
        end
      end

    end  -- index
  end


  ---| Hex_make_cycles |---

  add_thread()
  
  if rand.odds(60) then add_thread() end
  if rand.odds(60) then add_thread() end

  -- loop until all threads are dead

  while #threads > 0 do
    
    grow_threads()
    grow_threads()
    grow_threads()

    if #threads == 0 or  rand.odds(2) then add_thread() end
    if #threads == 1 and rand.odds(5) then add_thread() end

  end
end


function Hex_trim_leaves()
  
  local function trim_pass()
    local changes = 0

    for cx = 1, HEX_W do
    for cy = 1, HEX_H do
      local C = HEX_MAP[cx][cy]

      if C.kind != "used" then
        continue
      end

      if C:is_leaf() then
      
        C.kind = "wall"
        C.content = nil
        C.trimmed = true

        -- we keep C.thread

        changes = changes + 1
      end
    end
    end
 
    return (changes > 0)
  end


  ---| Hex_trim_leaves |---

  while trim_pass() do
    -- keep going until all nothing changes
  end
end


function Hex_check_map_is_valid()

  if CTF_MODE then
    -- ensure the starting cells survived

    for cx = 1, HEX_W do
      local C = HEX_MAP[cx][HEX_MID_Y]

      if C.trimmed then
        stderrf("Failed CTF connection test.\n")
        return false
      end
    end
  end

  -- generic size / volume checks

  -- TODO: consider counting "branch" cells

  local cx_min, cx_max = 999, -999
  local cy_min, cy_max = 999, -999

  local count = 0

  for cx = 1, HEX_W do
  for cy = 1, HEX_H do
    local C = HEX_MAP[cx][cy]

    if C.kind == "used" then
      count = count + 1

      cx_min = math.min(cx, cx_min)
      cy_min = math.min(cy, cy_min)

      cx_max = math.max(cx, cx_max)
      cy_max = math.max(cy, cy_max)
    end
  end
  end

  count = count / (HEX_W * HEX_H)

  local width  = (cx_max - cx_min + 1) / HEX_W
  local height = (cy_max - cy_min + 1) / HEX_H

  if CTF_MODE then
    count  = count * 2
    height = height * 2
  end

  gui.debugf("Volume: %1.3f  width: %1.2f  height: %1.2f\n", count, width, height)

  -- Note: no check on volume

  if width < 0.4 or height < 0.5 then
    stderrf("Failed size test.\n")
    return false
  end

  return true
end


function Hex_plan()
  -- keep trying until a plan comes together
  -- (mainly for CTF mode, which sometimes fails)

  repeat
    Hex_setup()
    Hex_starting_area()

    Hex_make_cycles()
    Hex_trim_leaves()

  until Hex_check_map_is_valid()
end


function Hex_add_rooms_CTF()
  --
  -- Algorithm:
  --
  --   1. setup rooms on the middle row
  --
  --   2. pick location for flag room
  --
  --   3. process each row away from that, pick room for each cell
  --      and occasionally create new rooms
  --

  local function new_room()
    local ROOM =
    {
      id = Plan_alloc_id("hex_room")

      cells = {}

f_mat = rand.pick({ "GRAY7", "MFLR8_3", "MFLR8_4", "FLAT1",
                    "TEKGREN2", "BROWN1", "BIGBRIK1",
                    "ASHWALL2", "ASHWALL4",
                    "FLAT14", "FLAT1_1", "FLAT2", "FLAT5_3",
                    "FLAT22", "FLAT4", "FLOOR1_7", "GATE1",
                    "FLOOR4_8", "LAVA1", "FWATER1", "NUKAGE1",
                    "GRNLITE1", "TLITE6_5", "STEP1", "SLIME09",
                    "SFLR6_1", "RROCK19", "RROCK17", "RROCK13",
                    "RROCK04", "RROCK02"
                    })
    }

    return ROOM
  end


  local function set_room(C, room)
    C.room = room

    table.insert(room.cells, c)

    if C.thread and not C.thread.room then
      C.thread.room = room
    end
  end


  local function initial_row()
    -- these must be mirrored horizontally, otherwise when the other
    -- half of the map is mirrored there would be a mismatch.

    local cy = HEX_MID_Y

    local last_room

    for cx = HEX_MID_X, 1, -1 do
      local C = HEX_MAP[cx][cy]

      if C.kind == "edge" then continue end

      if last_room and #last_room.cells == 1 and rand.odds(50) then
        set_room(C, last_room)
        continue
      end

      last_room = new_room()

      set_room(C, last_room)
    end

    -- do the mirroring

    for cx = 1, HEX_MID_X - 1 do
      local dx = HEX_MID_X + (HEX_MID_X - cx)

      local C = HEX_MAP[cx][cy]
      local D = HEX_MAP[dx][cy]

      if C.kind == "edge" then continue end

      assert(D.kind != "edge")

      set_room(D, C.room)
    end
  end


  local function bottom_cell_in_column(cx)
    for cy = 1, HEX_MID_Y - 8 do
      local C = HEX_MAP[cx][cy]

      if C.kind == "used" then return cy end
    end

    return nil  -- none at all
  end


  local function pick_flag_pos()
    local cx, cy

    repeat
      cx = math.random(1 + 2, HEX_W - 3)
    
      cy = bottom_cell_in_column(cx)
    until cy

    return cx, cy
  end


  local function plant_the_flag()
    -- determine middle of flag room
    -- (pick lowest of two tries)

    local cx,  cy  = pick_flag_pos()
    local cx2, cy2 = pick_flag_pos()

    if cy2 < cy then
      cx, cy = cx2, cy2
    end


    -- apply a vertical adjustment

    if cy == 1 then cy = cy + 1 end

    if cy > 2 and rand.odds(35) then cy = cy - 1 end

        if rand.odds(10) then cy = cy + 2
    elseif rand.odds(35) then cy = cy + 1
    end


    -- create the room

    local R = new_room()

    R.flag_room = true

    local C = HEX_MAP[cx][cy]

    C.room = R

    for dir = 1,6 do
      C.neighbor[dir].room = R
    end


    -- mark location of flag

    local fy = cy - rand.sel(40, 1, 0)

    local F = HEX_MAP[cx][fy]

    F.content = "FLAG"
  end


  local function rooms_from_threads()  -- NOT USED
    for cx = 1, HEX_W do
    for cy = 1, HEX_MID_Y - 1 do
      local C = HEX_MAP[cx][cy]

      if not (C.thread and not C.trimmed) then continue end

      if not C.thread.room then
        C.thread.room = new_room()
      end

      set_room(C, C.thread.room)
    end
    end
  end


  local function process_row(cy)
    local last_new_room

    for cx = 1, HEX_W do
      local C = HEX_MAP[cx][cy]

      -- already set?
      if C.room then continue end

      if C.kind == "edge" then continue end

      -- allow a new room to occupy _two_ horizontal spots

      if last_new_room and #last_new_room.cells == 1 and rand.odds(30) then
        set_room(C, last_new_room)
        continue
      end

      -- occasionally create a new room (unless last cell was new)

      if not last_new_room and rand.odds(15) then
        last_new_room = new_room()
        set_room(C, last_new_room)
        continue
      end

      last_new_room = nil

      -- otherwise we choose between two above neighbors (diagonals)

      local N4 = C.neighbor[4]
      local N5 = C.neighbor[5]
      local N6 = C.neighbor[6]

      if N4 and not N4.room then N4 = nil end
      if N5 and not N5.room then N5 = nil end
      if N6 and not N6.room then N6 = nil end

      assert(N4 or N6)

      local T
      if C.thread and not C.trimmed then T = C.thread end

      if N5 and rand.odds(sel(N5.thread == T, 90, 20)) then
        C.room = N5.room
        continue
      end

      if not N4 then C.room = N6.room ; continue end
      if not N6 then C.room = N4.room ; continue end

      if N4.room == N6.room then C.room = N4.room ; continue end

      -- tend to prefer the same thread
      local prob = 50

      if N4.thread == T then prob = prob + 40 end
      if N6.thread == T then prob = prob - 40 end

      C.room = rand.sel(prob, N4.room, N6.room)
    end
  end


  ---| Hex_add_rooms_CTF |---

  initial_row()

  plant_the_flag()

--  rooms_from_threads()

  for cy = HEX_MID_Y - 1, 1, -1 do
    process_row(cy)
  end
end


function Hex_add_rooms()

  ---| Hex_add_rooms |---

  if CTF_MODE then
    Hex_add_rooms_CTF()
    return
  end
end


function Hex_mirror_map()
  for cx = 1, HEX_W do
  for cy = 1, HEX_MID_Y - 1 do
    local C = HEX_MAP[cx][cy]

    local dx = (HEX_W - cx) + (cy % 2)
    local dy = (HEX_H - cy) + 1

    if dx < 1 then continue end

    local D = HEX_MAP[dx][dy]

    D.kind = C.kind
    D.room = C.room

    D.content = C.content
    D.entity  = C.entity
  end
  end
end


function Hex_shrink_edges()
  -- compute a distance from each used cell.
  -- free cells which touch an edge and are far away become edge cells.

  local top_H = sel(CTF_MODE, HEX_MID_Y - 1, HEX_H)

  local function mark_cells()
    for cx = 1, HEX_W do
    for cy = 1, top_H do
      local C = HEX_MAP[cx][cy]

      if C.kind == "used" then
        C.used_dist = 0
      end
    end
    end
  end


  local function sweep_cells()
    local changes = 0

    for cx = 1, HEX_W do
    for cy = 1, top_H do
      local C = HEX_MAP[cx][cy]

      if C.kind == "edge" then continue end

      local dist = C:used_dist_from_neighbors()

      if not dist then continue end

      dist = dist + 1

      if not C.used_dist or dist < C.used_dist then
        C.used_dist = dist

        changes = changes + 1
      end
    end
    end

stderrf("sweep_cells : changes = %d\n", changes)
    return (changes > 0)
  end


  local function set_edge(C)
    C.kind = "edge"

    -- FIXME room ??
  end


  local function grow_edges()
    local changes = 0

    for cx = 1, HEX_W do
    for cy = 1, top_H do
      local C = HEX_MAP[cx][cy]

      if C.used_dist and
         C.used_dist > 3 and
         C:touches_edge()
      then
        set_edge(C)

        C.used_dist = nil

        changes = changes + 1
      end
    end
    end

stderrf("grow_edges : changes = %d\n", changes)
    return (changes > 0)
  end


  ---| Hex_shrink_edges |---

  mark_cells()

  while sweep_cells() do end

  while grow_edges() do end
end


function Hex_build_all()
  for cx = 1, HEX_W do
  for cy = 1, HEX_H do
    local C = HEX_MAP[cx][cy]

    C:build()
  end
  end
end


function Hex_create_level()
  LEVEL.sky_light = 192
  LEVEL.sky_shade = 160

  Hex_plan()

  Hex_shrink_edges()

  Hex_add_rooms()

  -- Hex_place_stuff()

  if CTF_MODE then
    Hex_mirror_map()
  end

  Hex_build_all()
end

