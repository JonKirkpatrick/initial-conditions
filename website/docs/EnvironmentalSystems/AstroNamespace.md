# High-Level Architecture: `Astro` Namespace

The `Astro` namespace serves as the engine's core astronomical and temporal tracking module. It provides high-precision celestial body positioning, skybox orientation transforms, solar color temperature scaling, and game-time progression. Rather than acting as a scene component, `Astro` acts as a stateless, analytical utility namespace designed to produce a unified frame snapshot (`Astro::State`) that downstream graphics shaders and skybox renderers consume.

---

## 1. Key Data Structures

* **`Astro::Time`**: Stores primary astronomical time metrics, including full Julian Date (`julianDate`), Julian centuries since epoch J2000.0 (`julianCenturies`), Greenwich Mean Sidereal Time (`gmst`), and Local Sidereal Time (`lst`).


* **`Astro::AltAz`**: Encapsulates horizontal coordinate system values—Elevation/Altitude (`elevationRad`) and Azimuth (`azimuthRad`) in radians.


* **`Astro::TimeCalculationResult`**: Serves as an intermediate container returned by `computeSiderealTime`, bundling the populated `Time` struct alongside a calibrated starfield alignment offset (`epochOffset`).


* **`Astro::State`**: The primary state payload. It aggregates astronomical time, normalized directional vectors for both the Sun and Moon, the direct sunlight color vector, and a $3 \times 3$ row-major transformation matrix for the background starfield.



---

## 2. Mathematical Systems & Coordinate Transformations

### Coordinate Mapping Convention

The engine maps horizontal celestial coordinates ($\text{Elevation } E, \text{Azimuth } A$) directly into a normalized 3D world space direction vector via `altAzToDirection`:

* **$X$-axis**: East $(+)$ / West $(-)$

* **$Y$-axis**: Up $(+)$ / Down $(-)$

* **$Z$-axis**: South $(+)$ / North $(-)$


$$\vec{D} = \begin{pmatrix} -\cos(E) \cdot \sin(A) \\ \sin(E) \\ -\cos(E) \cdot \cos(A) \end{pmatrix}$$

### Sidereal Time & Julian Epoch Alignment

`computeSiderealTime` calculates the exact Julian Day Number ($\text{JDN}$) from calendar dates. It adjusts local time to UTC based on longitude, calculates elapsed Julian centuries since J2000.0, and evaluates $\text{GMST}$ and $\text{LST}$. To align starfield textures correctly with sky renderers, it applies a $-110^\circ$ calibration offset (`TEXTURE_CALIBRATION_DEGREES`) to derive the target `epochOffset`.

### Truncated Meeus Lunar Series

`computeMoonDirection` evaluates lunar positioning analytically using a truncated series adapted from Jean Meeus's *Astronomical Algorithms* (Chapter 47).

1. Calculates fundamental lunar arguments: mean longitude ($L_m$), mean anomaly ($M$), solar mean anomaly ($M_s$), and argument of latitude ($F$) based on Julian centuries $T$.


2. Computes periodic perturbations to determine ecliptic longitude ($\lambda$) and latitude ($\beta$).


3. Converts ecliptic coordinates to equatorial Right Ascension ($\text{RA}$) and Declination ($\text{Dec}$) using the obliquity of the ecliptic ($\epsilon \approx 23.439^\circ$).


4. Translates Right Ascension into Hour Angle ($\text{HA} = \text{LST} - \text{RA}$).


5. Transforms $(\text{HA}, \text{Dec})$ to Altitude/Azimuth ($\text{AltAz}$) and extracts the final normalized 3D direction vector.



### Starfield Transformation Matrix

`computeStarRotationMatrix` builds a $3 \times 3$ row-major rotation matrix based on observer latitude, longitude, and the calculated `epochOffset`. This matrix is passed straight to the GPU skybox shaders to correctly orient and rotate the starfield background sphere over time.

---

## 3. Directional Lighting & Solar Color Dynamics

The direct light contribution (`Astro::State::sunColor`) features a dynamic dynamic color temperature and intensity curve calculated during `calculateState`:

* **Elevation Factor**: Derived by clamping solar elevation: $\text{factor} = \text{clamp}\left(\frac{E_\text{deg} + 12^\circ}{90^\circ}, 0, 1\right)$.


* **Warmth Scaling**: $\text{warmth} = 1.0 - (\text{factor} \cdot 0.75)$, creating warmer red/amber hues near twilight and cooler white light at zenith.


* **Color Output Vector ($\text{RGBA}$ / $\text{RGBW}$)**:
* $\text{Red} = 1.00$

* $\text{Green} = 0.90 + (\text{warmth} \cdot 0.10)$

* $\text{Blue} = 0.65 + (\text{warmth} \cdot 0.30)$

* $\text{Intensity Factor } (W) = \text{factor} \cdot 1.25 + 0.25$




---

## 4. Simulation Execution Flow & Frame Lifecycle

At execution time, game loops interface with `Astro` in two primary steps:

```
[ Frame Delta Time (dt) ]
          │
          ▼
┌───────────────────────────┐
│ Astro::advanceCalendar()  │  ---> Updates Clock, Days, Months, Leap Years
└─────────┬─────────────────┘
          │
          ▼
┌───────────────────────────┐
│ Astro::calculateState()   │  ---> Computes Sun/Moon Vectors, Light Color & Star Matrix
└─────────┬─────────────────┘
          │
          ▼
┌───────────────────────────┐
│      Astro::State         │  ---> Streamed to Uniform Buffers / Render Pipelines
└───────────────────────────┘

```

1. **Temporal Advance (`advanceCalendar`)**: Increments fractional hours based on delta time and time scale factors (`realSecondsPerHour`). Automatically handles month-end bounds, leap years, and annual rollovers.


2. **State Generation (`calculateState`)**: Acts as a central orchestrator. Evaluates sidereal metrics, calculates solar declination/elevation, updates starfield transforms, and executes Meeus lunar positioning. Returns a self-contained `Astro::State` struct ready for pass-through to render pipelines.