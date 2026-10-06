# Screenshot Guide

fnTech3 keeps every game's familiar screenshot commands and adds more: your
own file names, a choice of formats, camera notes for retaking a shot, a
watermark, map preview pictures and six-sided cube maps. Bind `screenshot` to
a key, or type it in the console.

## Where screenshots go

Pictures are saved in the `screenshots` folder of the game's folder in your
home folder -- for example `baseq3\screenshots` for Quake III Arena and Quake
Live, `main\screenshots` for Return to Castle Wolfenstein, and `etmain`,
`baseEF`, `base` or `fakk` for Enemy Territory, Elite Force, Alice and
F.A.K.K.2 -- or in the mod's folder when you play a mod. The
[Games guide](GAMES.md#where-files-are-read-and-written) says where your home
folder is.

## Taking screenshots

- `screenshot`: saves a PNG picture.
- `screenshotJPEG`, `screenshotTGA` or `screenshotBMP`: saves a JPEG, TGA or
  BMP picture instead. `screenshotPNG` is the same as `screenshot`.
  `r_screenshotJpegQuality` sets the JPEG quality, from `1` to `100`
  (default `90`).
- `screenshot <name>`: saves as `screenshots/<name>.png` instead of a
  name chosen for you.
- `screenshot silent`: saves without the message on screen.
- `screenshotBMP clipboard`: copies the picture to the clipboard instead of
  saving a file, ready to paste into another program.
- `screenshot levelshot`: saves a map preview picture (see
  [Levelshots](#levelshots)).
- `screenshot cubemap`: saves a cube map (see [Cube Maps](#cube-maps)).

To take a screenshot with a key, bind it in the console, for example
`bind F12 screenshot`.

## Choosing file names

Without a name of your own, fnTech3 names each picture from
`r_screenshotNameFormat`. The default, `"shot-{date}-{time}"`, gives names
like `shot-20261006-213045.png`. The file's ending always matches the command
you used, and if a name is already taken fnTech3 adds a number.

You can build names from these pieces:

- `{map}`: the map's name.
- `{date}`: today's date, as `YYYYMMDD`.
- `{time}`: the time, as `HHMMSS`.
- `{datetime}`: both, as `YYYYMMDD-HHMMSS`.
- `{iter}`: a counting number, starting at `0`; `{iter:4}` pads it to four
  digits, like `0007`.
- `{cmd}`: the command used, such as `screenshot` or `screenshotJPEG`.
- `{face}`: the side of a cube map, for cube map pictures.
- `{type}`: the file type, such as `png` or `jpg`.

For example:

```cfg
seta r_screenshotNameFormat "shot-{map}-{datetime}"
```

```cfg
seta r_screenshotNameFormat "{map}-{cmd}-{iter:4}"
```

Characters that file names cannot hold are replaced for you.

## Notes for retaking a shot

With `r_screenshotWriteViewpos 1`, fnTech3 saves a small text file beside
each picture, with the same name ending in `.txt`. It records the map, where
the camera stood and which way it looked, and a `setviewpos` command that
puts you back in exactly the same spot:

```text
map q3dm17
origin 128.000 -64.000 512.000
angles -10.000 90.000 0.000
captureMode sdr-srgb
captureColorSpace sdr-srgb
setviewpos 128.000 -64.000 512.000 -10.000 90.000 0.000
```

That makes it easy to retake a shot later, or to compare the same view in
different renderers.

## Watermarks

fnTech3 can stamp your own logo onto every screenshot.

- `r_screenshotWatermark`: the picture to use -- PNG, TGA, JPEG or BMP --
  given as a path inside the game's files, loose or in a `.pk3`. fnTech3
  does not come with one.
- `r_screenshotWatermarkAlignment`: where it goes inside a centred 4:3 area of
  the screenshot.
- `r_screenshotWatermarkScreenAlignment`: where that 4:3 area sits in the
  whole screenshot.
- `r_screenshotWatermarkMargin`: the gap from the edge, in pixels.

Both alignments take `top-left`, `top`, `top-right`, `left`, `center`,
`right`, `bottom-left`, `bottom` or `bottom-right`. Using the 4:3 area keeps
the watermark in the same place on 4:3 and widescreen screenshots. The
watermark is drawn at its own size, never stretched. If it cannot be loaded,
the screenshot is still saved and the console warns you. Cube maps never get
a watermark.

```cfg
seta r_screenshotWatermark "gfx/watermarks/my-mark.png"
seta r_screenshotWatermarkAlignment "bottom-right"
seta r_screenshotWatermarkScreenAlignment "center"
seta r_screenshotWatermarkMargin "24"
```

## Levelshots

A *levelshot* is a map's preview picture, the one a game shows while the map
loads or in its map list. `screenshot levelshot` saves one as
`levelshots/<map>.tga`. Unlike the originals' small fixed size, it keeps the
full picture unless you ask otherwise:

- `r_levelshotSize`: the size to save. Leave it empty to keep the picture's
  size; `512` makes a 512x512 square, and `1024x512` any shape you like.
- `r_levelshotDownscale`: shrinks the picture when `r_levelshotSize` is
  empty: `1` full size, `2` half, `4` a quarter.
- `r_levelshotSourceAspect`: crops the middle of the screen to a shape first,
  such as `4:3`, `16:9` or `1:1`. Leave it empty to keep the whole screen.
- `r_levelshotHideHud`: `1` (the default) leaves the HUD out of the picture;
  `0` keeps it.
- `r_levelshotHideViewWeapon`: `1` (the default) leaves your weapon out; `0`
  keeps it.

The HUD and weapon can be left out in Quake III Arena, Quake Live and Elite
Force, and the HUD in Return to Castle Wolfenstein. Enemy Territory, Alice
and F.A.K.K.2 always include both, and Return to Castle Wolfenstein its
weapon. Elite Force single player's loading screen shows the levelshot when
it finds one, and otherwise a picture of the last frame.

For example, a 4:3 crop at half size:

```cfg
seta r_levelshotSourceAspect "4:3"
seta r_levelshotSize ""
seta r_levelshotDownscale "2"
screenshot levelshot
```

Or a 512x512 square from the middle of the screen:

```cfg
seta r_levelshotSourceAspect "1:1"
seta r_levelshotSize "512"
screenshot levelshot
```

Alice and F.A.K.K.2 also have their original command
`saveshot <name> <width> <height>`, which saves a small greyscale
`screenshots/<name>.tga`; the games use it for their saved-game pictures.

## Cube Maps

A *cube map* is six square pictures that together show everything around
you -- front, back, left, right, up and down -- as used for skyboxes and
reflections. Stand where you want the centre to be and type
`screenshot cubemap`; add a name to choose the files' names:

```cfg
screenshot cubemap env/q3dm1-sky
```

That saves `screenshots/env/q3dm1-sky-front.png`, and the same with `back`,
`left`, `right`, `top` and `bottom`. `screenshotJPEG cubemap`,
`screenshotTGA cubemap` and `screenshotBMP cubemap` save other formats, and
`screenshot cubemap silent` saves without the message.

All six pictures are taken from the same moment, so nothing moves between
them, and each is an exact 90-degree square view, so their edges meet.
Effects that depend on the view -- glow, motion blur and screen distortion --
are left out so the edges match; brightness and colour grading are kept. The
HUD and your weapon are left out too, as `r_levelshotHideHud` and
`r_levelshotHideViewWeapon` say; set either to `0` and it appears in the
`front` picture only. If
your naming pattern has no `{face}`, the side's name is added for you.

Cube maps work in the GLx and Vulkan renderers. RTX cannot take them yet; it
says so and saves nothing, though ordinary screenshots and levelshots work.

## Colour and HDR

Screenshots, levelshots, cube maps, clipboard pictures and video frames are
always saved as ordinary (SDR) pictures, even when you play with HDR, so they
look right everywhere and match earlier fnTech3 pictures.

`r_screenshotCaptureMode`, in GLx and RTX, records which kind of picture you
asked for:

- `0`: an ordinary picture. This is the default.
- `1` and `2`: reserved for saving real HDR pictures in a future version. For
  now they save an ordinary picture and print a warning.

With `r_screenshotWriteViewpos 1`, the text file beside each picture also
records which kind was asked for and saved.

## Recommended starting points

An organised screenshot collection:

```cfg
seta r_screenshotNameFormat "{map}-{datetime}-{iter:3}"
seta r_screenshotWriteViewpos "1"
```

Screenshots with your logo:

```cfg
seta r_screenshotNameFormat "{map}-{datetime}"
seta r_screenshotWatermark "gfx/watermarks/my-mark.png"
seta r_screenshotWatermarkAlignment "bottom-right"
seta r_screenshotWatermarkScreenAlignment "center"
seta r_screenshotWatermarkMargin "16"
```

Cube maps with clear names:

```cfg
seta r_screenshotNameFormat "{map}-{datetime}-{face}"
screenshot cubemap
```
