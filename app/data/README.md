# Observation data provenance

- `NGC.csv`, `NGC-addendum.csv`: unmodified OpenNGC database and addendum from
  Mattia Verga and contributors, revision `4e2398a263de663d83f7b86be80c368c34335764`,
  https://github.com/mattiaverga/OpenNGC, retrieved 2026-10-11. CC BY-SA 4.0;
  license in `third_party/notices/OpenNGC-LICENSE.txt`. Retain the Sources column.
  OpenNGC incorporates NED, HyperLEDA, SIMBAD/CDS, HEASARC and Harold Corwin's
  NGC/IC positions and notes; acknowledgements are in the upstream README.
  Runtime parsing excludes missing coordinates, duplicate and nonexistent entries.
- `world-land.json`: exterior polygons from Natural Earth 1:110m country geometry,
  rounded to 0.001 degrees and encoded as longitude/latitude pairs. Source:
  https://github.com/nvkelso/natural-earth-vector/blob/master/geojson/ne_110m_admin_0_countries.geojson,
  retrieved 2026-10-11. Natural Earth is public domain:
  https://www.naturalearthdata.com/about/terms-of-use/.

Preset city coordinates are approximate city centres at sea level. Online city
results use GeoNames via Open-Meteo with returned elevation and IANA time zone:
https://open-meteo.com/en/docs/geocoding-api. No weather is queried. Only user-entered
names are sent upon explicit search; no camera or image data is transmitted.
