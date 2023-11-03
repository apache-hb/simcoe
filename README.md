# simcoe

## todos

* migrate thread scheduler into ThreadService
    * implement fallback layer incase ThreadService cant get geometry
    * simplify thread handles

* implement optional deps

* create DepotService to handle io

* find a way to run a subprocess as administrator for RyzenMonitor
    * process shared memory

* prevent xinput from polling and failing every frame
* windows isnt alerting to missing dlls for some reason

## random bits

* get std::formatter support into toml++
* have a look at how the ryzenmonitor sdk+driver works
* clean up warnings in freetype and harfbuzz
* upstream libpng meson changes
* figure out why meson+windows+shared libraries are broken by default
