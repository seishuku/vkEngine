# BModel File Format Specification

## 1. Overview

`BModel` (`.bmodel`) is a binary 3D model format used by vkEngine.

The format contains:

* Materials
* Meshes and triangle indices
* Vertex positions
* Optional texture coordinates
* Optional tangent space vectors
* Optional skeletal bones
* Optional per-vertex bone weights

The file begins with a four-byte file identifier

All integer and floating-point values are written little endian.

---

# 2. File Layout

General file layout:

```text
BMDL_MAGIC

uint32_t numMaterial

    [material 0]
    ...
    [material numMaterial-1]

uint32_t numMesh

    [mesh 0]
    ...
    [mesh numMesh-1]

uint32_t numVertex

    [vertex data chunks...]
```

Vertex data portion consists of tagged chunks. Chunks are identified by a four-byte magic value and may appear in any order.

Current vertex chunks are:

```text
VERT    Vertex positions
TEXC    Texture coordinates
TANG    Tangents
BNRM    Binormals
NORM    Normals
BONE    Skeleton
BWGT    Vertex bone weights
```

The loader continues reading chunks until the end of the file.

Unknown chunk identifiers should be rejected and cause an error during loading.

---

# 3. Magic Values

Each magic value is stored as a `uint32_t`.

The values are constructed from four ASCII characters, with the first character occupying the least-significant byte.

| Identifier | ASCII  | Purpose             |
| ---------- | ------ | ------------------- |
| `BMDL`     | `BMDL` | File header         |
| `MESH`     | `MESH` | Mesh record         |
| `MATL`     | `MATL` | Material record     |
| `VERT`     | `VERT` | Vertex positions    |
| `TEXC`     | `TEXC` | Texture coordinates |
| `TANG`     | `TANG` | Tangent vectors     |
| `BNRM`     | `BNRM` | Binormal vectors    |
| `NORM`     | `NORM` | Normal vectors      |
| `BONE`     | `BONE` | Bone/skeleton data  |
| `BWGT`     | `BWGT` | Vertex bone weights |

The constants are defined as:

```c
BMDL_MAGIC = 'B'|'M'<<8|'D'<<16|'L'<<24;
MESH_MAGIC = 'M'|'E'<<8|'S'<<16|'H'<<24;
MATL_MAGIC = 'M'|'A'<<8|'T'<<16|'L'<<24;
VERT_MAGIC = 'V'|'E'<<8|'R'<<16|'T'<<24;
TEXC_MAGIC = 'T'|'E'<<8|'X'<<16|'C'<<24;
TANG_MAGIC = 'T'|'A'<<8|'N'<<16|'G'<<24;
BNRM_MAGIC = 'B'|'N'<<8|'R'<<16|'M'<<24;
NORM_MAGIC = 'N'|'O'<<8|'R'<<16|'M'<<24;
BONE_MAGIC = 'B'|'O'<<8|'N'<<16|'E'<<24;
BWGT_MAGIC = 'B'|'W'<<8|'G'<<16|'T'<<24;
```

---

# 4. Strings

Strings are stored as null-terminated character sequences.

The loader reads a string until either:

1. A `'\0'` byte is encountered, or
2. The destination buffer reaches its maximum size.

The currently defined string fields have a maximum size of 256 bytes:

```c
char name[256];
char materialName[256];
char texture[256];
```

Writers should ensure that strings are null-terminated and fit within the corresponding 256-byte field.

---

# 5. Material Section

The material section begins immediately after the material count.

```text
uint32_t numMaterial
```

If `numMaterial` is non-zero, exactly `numMaterial` material records follow.

Each material record has the following layout:

```text
uint32_t magic              // MATL

char name[]                 // null-terminated

float ambient[3]
float diffuse[3]
float specular[3]
float emission[3]

float shininess

char texture[]              // null-terminated
```

### Material fields

| Field     | Type       | Description           |
| --------- | ---------- | --------------------- |
| Magic     | `uint32_t` | Must be `MATL`        |
| Name      | string     | Material name         |
| Ambient   | `float[3]` | Ambient color         |
| Diffuse   | `float[3]` | Diffuse color         |
| Specular  | `float[3]` | Specular color        |
| Emission  | `float[3]` | Emission color        |
| Shininess | `float`    | Material shininess    |
| Texture   | string     | Texture filename/name |

The material structure corresponds to:

```c
typedef struct
{
    char name[256];
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    vec3 emission;
    float shininess;
    char texture[256];
} BModel_Material_t;
```

---

# 6. Mesh Section

The material section is followed by:

```text
uint32_t numMesh
```

Exactly `numMesh` mesh records follow.

Each mesh record has the layout:

```text
uint32_t magic              // MESH

char name[]                 // null-terminated
char materialName[]         // null-terminated

uint32_t numFace

uint32_t face[numFace * 3]
```

### Mesh fields

| Field         | Type         | Description                        |
| ------------- | ------------ | ---------------------------------- |
| Magic         | `uint32_t`   | Must be `MESH`                     |
| Name          | string       | Mesh name                          |
| Material name | string       | Name of material used by this mesh |
| Face count    | `uint32_t`   | Number of triangles                |
| Indices       | `uint32_t[]` | Three indices per triangle         |

Each face consists of exactly three 32-bit vertex indices:

```text
face[0] face[1] face[2]       // triangle 0
face[3] face[4] face[5]       // triangle 1
...
```

```text
index_count = numFace * 3
```

The loader matches `materialName` against the material names to determine the material index.

---

# 7. Vertex Data

After mesh records comes:

```text
uint32_t numVertex
```

This is the number of vertices shared by all meshes.

The remaining data consists of tagged vertex-data chunks.

The loader allocates arrays based on `numVertex`, and every vertex attribute chunk contains exactly `numVertex` elements.

---

## 7.1 Vertex Positions — `VERT`

Layout:

```text
uint32_t magic              // VERT

float vertex[numVertex][3]
```

Each vertex consists of three floats:

```text
x
y
z
```

Total data size:

```text
numVertex * 3 * sizeof(float)
```

Position data is the only mandatory pre-vertex chunk.

---

## 7.2 Texture Coordinates — `TEXC`

Layout:

```text
uint32_t magic              // TEXC

float UV[numVertex][2]
```

Each vertex has:

```text
u
v
```

Total data size:

```text
numVertex * 2 * sizeof(float)
```

Texture coordinates are optional.

---

## 7.3 Tangents — `TANG`

Layout:

```text
uint32_t magic              // TANG

float tangent[numVertex][3]
```

Each tangent consists of three floats.

This chunk is the part of the "tangent space", typically used for normal-mapping and is optional.

---

## 7.4 Binormals — `BNRM`

Layout:

```text
uint32_t magic              // BNRM

float binormal[numVertex][3]
```

Each binormal consists of three floats.

This chunk is the part of the "tangent space", typically used for normal-mapping and is optional.

---

## 7.5 Normals — `NORM`

Layout:

```text
uint32_t magic              // NORM

float normal[numVertex][3]
```

Each normal consists of three floats.

This chunk is the vector perpendicular to the triangle surface, typically used for vertex lighting and is also part of the "tangent space" and is optional.

---

# 7.6 Vertex Bone Weights — `BWGT`

The `BWGT` chunk contains skinning information for each vertex and is optional.

Layout:

```text
uint32_t magic              // BWGT

struct
{
    uint32_t bone[4];
    float weight[4];
} weight[numVertex];
```

There are exactly four possible bone influences per vertex.

The chunk contains exactly `numVertex` records.

---

# 8. Skeleton — `BONE`

The `BONE` chunk contains the model skeleton bind pose and is optional.

Each bone's `position`/`orientation` are stored **relative to its parent bone**, not in world/scene space — the same convention used by `BAnim_BoneFrame_t` in the animation format. A root bone (`parent == -1`) is relative to the model's own origin.

A loader reconstructs a bone's world-space bind matrix by composing it with its parent's already-computed world matrix:

```text
world[i] = parent >= 0 ? local[i] * world[parent] : local[i]
```

Layout:

```text
uint32_t magic              // BONE

uint32_t numBone

    for each bone:
        char name[]         // null-terminated
        int32_t parent
        float position[3]
        float orientation[4]
```

### Bone fields

| Field       | Type       | Description                                     |
| ----------- | ---------- | ------------------------------------------------ |
| Name        | string     | Bone name                                       |
| Parent      | `int32_t`  | Parent bone index                               |
| Position    | `float[3]` | Bone position, relative to parent               |
| Orientation | `float[4]` | Bone orientation quaternion, relative to parent |

A parent value of `-1` represents a root bone.

Note: Quaternion is stored as a typical VEC4 type (XYZW), not the conventional WXYZ.
---

# 9. Chunk Ordering

The loader does not require a particular ordering for the vertex-data chunks.

After `numVertex`, it repeatedly reads:

```text
uint32_t magic
```

and dispatches based on the magic value.

Recognized chunks can therefore be written in any order.

Unknown chunk identifiers can't be safely skipped by the loader and should generate a loading error, care should be also taken to check for unexpected EOF while reading in case of corrupted/invalid files.

---

# 10. Typical Complete File

A typical skinned model might therefore look like:

```text
BMDL

numMaterial
    MATL
        name
        ambient
        diffuse
        specular
        emission
        shininess
        texture

numMesh
    MESH
        name
        materialName
        numFace
        face indices...

numVertex

VERT
    vertex positions...

TEXC
    texture coordinates...

TANG
    tangent vectors...

BNRM
    binormal vectors...

NORM
    normal vectors...

BWGT
    vertex 0 weights
    vertex 1 weights
    ...

BONE
    numBone
    bone 0
        name
        parent
        position
        orientation
    bone 1
        ...
```

---

# 11. Static Model Example

A minimal static model could contain:

```text
BMDL

1                       // numMaterial

MATL
    "Material"
    ambient[3]
    diffuse[3]
    specular[3]
    emission[3]
    shininess
    "texture.png"

1                       // numMesh

MESH
    "Cube"
    "Material"
    12                  // numFace
    36 indices

8                       // numVertex

VERT
    8 × vec3 positions
```

If texture coordinates are present:

```text
TEXC
    8 × vec2 UVs
```

---

# 12. Skinned Model Example

A skinned model adds:

```text
BWGT

numVertex × weights

BONE

numBone

bone records...
```

The bone indices in `BWGT` refer directly to the zero-based indices of the bones in the `BONE` chunk.

For example:

```text
BONE

3

bone 0: Root
bone 1: Spine
bone 2: Arm

BWGT

vertex 0:
    bone[0]   = 0
    bone[1]   = 1
    bone[2]   = 0
    bone[3]   = 0

    weight[0] = 0.7
    weight[1] = 0.3
    weight[2] = 0.0
    weight[3] = 0.0
```

---

# 13. Binary Data Summary

The following table summarizes the serialized data.

| Chunk     | Header | Data                                     |
| --------- | ------ | ---------------------------------------- |
| File      | `BMDL` | Material and mesh sections               |
| Material  | `MATL` | Name, colors, shininess, texture         |
| Mesh      | `MESH` | Name, material name, triangle indices    |
| Positions | `VERT` | `numVertex × vec3`                       |
| UVs       | `TEXC` | `numVertex × vec2`                       |
| Tangents  | `TANG` | `numVertex × vec3`                       |
| Binormals | `BNRM` | `numVertex × vec3`                       |
| Normals   | `NORM` | `numVertex × vec3`                       |
| Weights   | `BWGT` | `numVertex × 4 bone indices + 4 weights` |
| Skeleton  | `BONE` | Bone hierarchy and transforms            |

---

# 14. Important Format Characteristics

### No version field

The current format has no explicit version number.

### No explicit chunk sizes

Chunks contain no byte-size field. The loader knows how much data to consume from the chunk type and `numVertex`/`numBone`/`numFace`.

### No explicit endianness marker

The format uses little endian representation.

### Vertex arrays are global

Vertices are stored once at the model level. Meshes reference them through their index arrays.

### Meshes are triangle-only

Every face consists of exactly three indices.

### Skinning is limited to four influences

Every vertex weight record contains exactly four bone indices and four weights.

### Tangent space is optional

The loader can generate it from vertices and UV coordinates, if it chooses to.

### Bone transforms are parent-relative, and parent-ordered

`BONE` chunk transforms are relative to each bone's parent, not world space, and bones must appear after their parent in the array (`parent < index`). A loader builds world-space bind matrices with a single forward pass over the array; it does not need to sort or recurse.

---

# 15. Conceptual Grammar

The format can be summarized by the following pseudo-grammar:

```text
BModel =
    BMDL_MAGIC
    MaterialCount
    Material*
    MeshCount
    Mesh*
    VertexCount
    VertexChunk*

Material =
    MATL_MAGIC
    String
    Vec3
    Vec3
    Vec3
    Vec3
    Float
    String

Mesh =
    MESH_MAGIC
    String
    String
    FaceCount
    UInt32[FaceCount * 3]

VertexChunk =
      VERT_MAGIC  Float[VertexCount * 3]
    | TEXC_MAGIC  Float[VertexCount * 2]
    | TANG_MAGIC  Float[VertexCount * 3]
    | BNRM_MAGIC  Float[VertexCount * 3]
    | NORM_MAGIC  Float[VertexCount * 3]
    | BWGT_MAGIC  Weight[VertexCount]

BoneData =
    BONE_MAGIC
    BoneCount
    Bone*

Bone =
    String
    Int32
    Float[3]
    Float[4]

Weight =
    UInt32[4]
    Float[4]
```
