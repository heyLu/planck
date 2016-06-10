# ton

> The ton to your [planck][]

`ton` is an attempt to bring [planck][] to Linux (and potentially other systems).

**Warning:** *This is very much a work in progress.  Lots of stuff is still missing, things might not work, ...  I'm not sure if this will work out, so the project might be abandoned.  I'd still love your help, though, so if you also want this to exist, feel free to look around and help!*

## Development

- install `webkit2gtk`, `libzip`
- `git clone https://github.com/mfikes/planck`
- `pushd planck/planck-cljs; ./script/build; ./script/bundle; popd`
- `cp -R planck/planck-cljs/out .`
- run `make`

## Implementation

- based on JavaScriptCore (from webkitgtk)
- uses planck's Clojure library
- access to native capabilities via a reimplementation of
    planck's Objective-C part

A lot of stuff is still missing (in fact most of it).

[planck]: https://github.com/heyLu/planck