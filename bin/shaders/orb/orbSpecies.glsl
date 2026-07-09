// ==============================================================================
// == orbSpecies.glsl ===========================================================
// == Per-species appearance data. Only the colour pass (orbCreature.frag)     ==
// == reads this.  The shadow pass has no use for iris/sclera colour, so it    ==
// == doesn't include this file and never binds slot 1.                        ==
// ==============================================================================
#ifndef ORB_SPECIES_GLSL
#define ORB_SPECIES_GLSL

struct SpeciesData {
    vec4 irisColourAndRadius;               // xyz = irisColour,    w = irisRadius
    vec4 scleraColour;                      // xyz = scleraColour,  w = spare
    vec4 tapetumColourAndPresence;          // xyz = tepetumColour, w = presence (0 or 1)
};

layout(std430, binding = 1) readonly buffer SpeciesBuffer {
    SpeciesData species[];
};

#endif // ORB_SPECIES_GLSL
