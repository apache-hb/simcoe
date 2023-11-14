# simcoe

## todos

* migrate thread scheduler into ThreadService
    * implement fallback layer incase ThreadService cant get geometry
    * simplify thread handles

* implement optional deps

* find a way to run a subprocess as administrator for RyzenMonitor
    * process shared memory

* prevent xinput from polling and failing every frame
* windows isnt alerting to missing dlls for some reason

* delay window creation until render context is ready

* display splashscreen while loading

* move imgui onto its own thread
* get imgui viewports working (will require talking between the imgui thread and main thread)
* get flecs working for game logic
* a few bits of physics

* simplify the input service
* logging categories
* better logging ui to help sort between threads and services, etc

* WER or watson support would be pretty funny to implement
* create the d3d12 device before the window opens
* fix borderless windowed mode
* move GameService into EditorService

## random bits

* join the windows desktop application program and get a cert

* get std::formatter support into toml++
* nag mesonbuild to support pulling a specific git commit 
* have a look at how the ryzenmonitor sdk+driver works internally
* upstream libpng meson changes
* figure out why meson+windows+shared libraries are broken by default
