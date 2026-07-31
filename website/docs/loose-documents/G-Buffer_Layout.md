### G-Buffer list, format and contents
| Buffer | Format | Contents |
|---|---|---|
| GB0, Albedo | RGBA8 | rgb: albedo, a: eyelidCoverage |
| GB1, Normal | RGBA16F | rgb: world-space normal, a: spare |
| GB2, Indices | RGBA8 | r: materialID 1, g: speciesIdx, b: materialID 2, a: mix of 1 and 2 |
| GB3, Retroreflection | RGBA8 | rg: gazeDir (octahedral), b: pupil eligibility, a: spare (reserved) |
| Depth | D32F | Position reconstruction, CSM cascades |