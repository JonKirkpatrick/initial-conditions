## A Digression from the TerrainOverhaul Path

At this stage in the terrain overhaul, it has become apparent that we will need to think carefully about how we determine where we are in the larger world and how we track the more abstract concept of where to look in the toroidal buffer for the tile and texel within the tile that will hold the height value for our current location.

### Open Questions:

1. Are these values that should be cached in a component?
2. If we decide that we don't need all the values cached, which ones warrant it?
3. Should we place our world origin at the upper left, or centre it?
4. What should we do at the boundary of our world?

### Some Fundamentals:

* Each tile represents a square of terrain 1024 metres on a side
* Each texel within a tile represents a square of terrain 4 metres on a side
* Each tile contains a 1 texel apron on the right and bottom of the tile
* We determine height by flooring our XZ coordinates then bilinear interpolation of the 2x2 block of texels to the right and below the current texel and using the fractional portion lost to flooring to determine the weight of the neighbouring texels. (That might be sloppy phrasing)
* We will have a block of 7x7 tiles available in memory, statically allocated at load
* We will replace a whole row or whole column upon crossing a tile boundary
* We will use %7 to index into the array
* The visible window into the world and seen by the shaders is the centre 5x5 block of tiles

### Musings on the Open Questions:

1. Knowing the cost of computing the grid location will inform whether this should be cached or just computed on the fly.  In the future, I will be using the same 4m square resolution as a grid to simplify collision checking, so there will need to be some sort of register/deregister machinery for cheaper looping over the collision cells instead of per entity.
2. The current world position floored and modulo integer divided by 4 would simplify the collision cell registration and make the rest of the grid lookups pretty simple.
3. If we place it in the upper left we avoid negative coordinates, but we would need to reintroduce them if we ever had a fully streaming limitless open world.  That seems like something to plan for when the need arises rather than now.  In any event, it's not likely to be too much of an impediment.
4. I imagine we will not allow movement beyond the boundary, and from that I see two options:  We either have a void, or we mirror the tiles.  The void seems easier right now.

## The Plan for Today

* Our original Schema version has proven to be ill-suited to our task.  We need to come up with a new schema to better describe our game world and set us up for success further along the path.  Invariant things like the size of a tile and terrain coverage of a texel and the fact that our buffer is 7x7 need to be stripped out.  We need to know the world bounds in tile coordinates, and perhaps the upper left tile file name along with the lower right tile name.  They will always be called tile_x_y.bin, where x is the row and y is the column.  The world will be made up of a complete rectangle of such tiles.
* We have our terrain streamer header skeleton written, but have not implemented the actual C++ class yet.  Today I would like to implement an impotent version of it that doesn't load tiles or make use of the eventual machinery, but does allow us to implement the necessary math to determine which tiles to load and where they will be allocated in the buffer.  It should read a mainfest following the new JSON schema and pretend to setup our buffers.