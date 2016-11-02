# CS430 Project 4 - Recursive Raytracing

This project implements a recursive ratracing algorithm with lights (point lights, spot lights) and shadows that allows primitive objects (spheres, planes) defined in an input file in JSON format into a PPM image file.

### Building

```sh
$ git clone https://github.com/bjg96/cs430-project-4-recursive-raytracing.git
$ cd cs430-project-4-recursive-raytracing
$ make
```

### Usage

```sh
$ ./raytrace <render_width> <render_height> <input_scene> <output_file>
$        render_width: The width of the image to render
$        render_height: The height of the image to render
$        input_scene: The input scene file in a supported JSON format
$        output_file: The location to write the output PPM P6 image
$
$        Example: raytrace 1920 1080 scene.json out.ppm
```
