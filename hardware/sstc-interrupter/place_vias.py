import pcbnew

board = pcbnew.GetBoard()

# ==== Parameters ====
grid_step = pcbnew.FromMM(2.54)     # grid spacing
via_dia   = pcbnew.FromMM(0.5)     # via diameter
via_drill = pcbnew.FromMM(0.4)     # drill size
clearance = pcbnew.FromMM(0.25)    # min clearance

# Get GND netcode (fallback to 0 if missing)
try:
    gnd_netcode = board.GetNetcodeFromNetname("GND")
except:
    gnd_netcode = 0
print("Using GND netcode:", gnd_netcode)

# ==== Helper: check if position is free ====
def is_point_free(board, pos, clearance):
    return True
    # Check pads
    for module in board.GetModules():
        for pad in module.Pads():
            if pad.GetBoundingBox().Inflated(clearance).Contains(pos):
                return False
    
    # Check tracks + existing vias
    for track in board.GetTracks():
        if track.GetBoundingBox().Inflated(clearance).Contains(pos):
            return False
    
    return True

# ==== Place vias in a grid ====
def place_vias_grid(board, bbox, step):
    count = 0
    for x in range(bbox.GetLeft(), bbox.GetRight(), step):
        for y in range(bbox.GetBottom(), bbox.GetTop(), step):
            pos = pcbnew.VECTOR2I(x, y)
            if is_point_free(board, pos, clearance):
                via = pcbnew.VIA(board)
                via.SetPosition(pos)
                via.SetWidth(via_dia)
                via.SetDrill(via_drill)
                via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
                via.SetNetCode(gnd_netcode)  # assign to GND net (or 0 if not found)
                board.Add(via)
                count += 1
    print("Placed", count, "vias")

# ==== Run ====
bbox = board.GetBoardEdgesBoundingBox()
print("Board bounding box (mm):",
      bbox.GetLeft()/1e6, bbox.GetBottom()/1e6,
      bbox.GetRight()/1e6, bbox.GetTop()/1e6)

place_vias_grid(board, bbox, grid_step)
pcbnew.Refresh()

