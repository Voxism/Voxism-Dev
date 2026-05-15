import bpy
import os

# CHANGE THIS
obj_path = "/home/owen/School/CSC476/Voxism-Dev/resources/gun1painted.obj"

# Clear scene
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete()

verts = []
colors = []
faces = []

with open(obj_path, "r", encoding="utf-8") as f:
    for line in f:
        line = line.strip()

        if line.startswith("v "):
            parts = line.split()

            x = float(parts[1])
            y = float(parts[2])
            z = float(parts[3])

            # Goxel OBJ stores RGB after XYZ
            if len(parts) >= 7:
                r = float(parts[4])
                g = float(parts[5])
                b = float(parts[6])
            else:
                r, g, b = 1.0, 1.0, 1.0

            verts.append((x, y, z))
            colors.append((r, g, b, 1.0))

        elif line.startswith("f "):
            parts = line.split()[1:]
            face = []

            for p in parts:
                # Supports f v, f v/vt, f v/vt/vn, f v//vn
                index = p.split("/")[0]
                face.append(int(index) - 1)

            faces.append(face)

print("OBJ vertices:", len(verts))
print("OBJ colors:", len(colors))
print("OBJ faces:", len(faces))

# Create mesh
mesh = bpy.data.meshes.new("GoxelMesh")
mesh.from_pydata(verts, [], faces)
mesh.update()

obj = bpy.data.objects.new("GoxelObject", mesh)
bpy.context.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)

# Blender 3.0 vertex color layer
vcol_layer = mesh.vertex_colors.new(name="GoxelColor")

# Assign color per face corner
for poly in mesh.polygons:
    for loop_index in poly.loop_indices:
        vertex_index = mesh.loops[loop_index].vertex_index
        vcol_layer.data[loop_index].color = colors[vertex_index]

# Create material
mat = bpy.data.materials.new("Goxel_VertexColor_Material")
mat.use_nodes = True

nodes = mat.node_tree.nodes
bsdf = nodes.get("Principled BSDF")

# Blender 3.0-compatible vertex color node
vcol_node = nodes.new(type="ShaderNodeVertexColor")
vcol_node.layer_name = "GoxelColor"

mat.node_tree.links.new(vcol_node.outputs["Color"], bsdf.inputs["Base Color"])

obj.data.materials.append(mat)

# Set origin/view niceties
bpy.ops.object.origin_set(type="ORIGIN_GEOMETRY", center="BOUNDS")

# Save a .blend next to the OBJ
blend_path = os.path.splitext(obj_path)[0] + "_colored.blend"
bpy.ops.wm.save_as_mainfile(filepath=blend_path)

print("Saved:", blend_path)
print("Done.")