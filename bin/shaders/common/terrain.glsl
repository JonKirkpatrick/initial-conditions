layout(location = 400) uniform float            u_heightMax;
layout(location = 401) uniform vec2             u_terrainGridWorldOrigin;
layout(location = 402) uniform float            u_terrainTileWorldSize;
layout(Location = 403) uniform sampler2DArray   u_terrainHeightArray;
layout(Location = 404) uniform int              u_terrainSliceValid[81];