## Glyph and IMGUI Library

Glyph is a native image editing application built from scratch in c. The project is broken up into several modules. 
For instance the *render* module creates a Vulkan graphics pipeline and renders ui elements in batches, the *os* module provides a
system independent layer for managing resources, and the *ui* module provides an immediate-mode graphical user interface library.

![Editing in the glyph application](https://github.com/henryrossi/gallery/blob/main/resources/Glyph.png)


## Build Instructions

The project is built with the build.sh script, or build.bat on windows. For development, see the tools folder.
