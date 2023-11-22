# simcoe

## building

```sh
meson setup build --cross-file data/config/meson/base.ini --cross-file data/config/meson/xxx-platform.ini
```

## todos

* migrate thread scheduler into ThreadService
    * implement fallback layer incase ThreadService cant get geometry
    * simplify thread handles

* implement optional deps

* find a way to run a subprocess as administrator for RyzenMonitor
    * process shared memory

* prevent xinput from polling and failing every frame
* windows isnt alerting to missing dlls for some reason

* display splashscreen while loading without the help of msstore bootstrapper

* move imgui onto its own thread
* get imgui viewports working (will require talking between the imgui thread and main thread)
* a few bits of physics

* simplify the input service
* logging categories
* better logging ui to help sort between threads and services, etc

* WER or watson support would be pretty funny to implement

* fix borderless windowed mode
* move GameService into EditorService
* fix flickering when changing backbuffer count
* split windowing out of platform service

* task graph of some sort for init and deinit
* possibly extend to runtime as well

* reorg folders to be less cumbersome
* C++ modules might be nice to try out
* using a pch at least would be good
* need a custom asset packaging pipeline, meson is getting limiting
    * keep it simple
    * ideally we could generate ninja files as well

* resizing the window causes d3d12 errors
* use a font atlas for rendering UI

## random bits

* join the windows desktop application program and get a cert

* get std::formatter support into toml++
* nag mesonbuild to support pulling a specific git commit 
* have a look at how the ryzenmonitor sdk+driver works internally
* upstream libpng meson changes
* figure out why meson+windows+shared libraries are broken by default
* use https://github.com/microsoft/detours to stop the windows thread pool from spinning up
* patch all of our deps to properly handle clang-cl on windows, right now everything hates it in various ways
* modify meson to make `meson.version().version_compare` track across nodes
