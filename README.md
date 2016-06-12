# ton

> The ton to your [planck][]

`ton` is an attempt to bring [planck][] to Linux (and potentially other systems).

[planck]: https://github.com/mfikes/planck

**Warning:** *This is very much a work in progress.  Lots of stuff is still missing, things might not work, ...  I'm not sure if this will work out, so the project might be abandoned.  I'd still love your help, though, so if you also want this to exist, feel free to look around and help!*

## Development

- install `javascriptcoregtk`, `libzip`, `zlib`
- `make bundle-and-build`
- have fun: `./ton`

## Implementation

- based on JavaScriptCore (from webkitgtk)
- uses planck's Clojure library
- access to native capabilities via a reimplementation of
    planck's Objective-C part

A lot of stuff is still missing (in fact most of it).

## License

Copyright © 2016 Mike Fikes, Lucas Stadler and Contributors

Distributed under the Eclipse Public License either version 1.0 or (at your option) any later version.