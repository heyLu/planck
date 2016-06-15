# planck-c

This is an in-progress port of Planck to C, supporting multiple
platforms.

**Warning:** *This is very much a work in progress.  Lots of stuff is still missing, things might not work, ...  Feel free to port things from the Objective-C version of planck.*

## Development

- install `javascriptcoregtk`, `libzip`, `zlib`
- `make bundle-and-build`
- have fun: `./ton`

## Implementation

- based on JavaScriptCore (from webkitgtk)
- uses `planck-cljs`
- access to native capabilities via a reimplementation of
    planck's Objective-C part

A lot of stuff is still missing (in fact most of it).
