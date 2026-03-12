# CAP4730 – Assignment 3
## Group Members
Alexander Kim, Matthew Lozito

## Compilation

Change the following paths in the Makefile to wherever glfw and glew are installed on your machine (should work out of the box if you are on mac):

-I/opt/homebrew/opt/glew/include \
-I/opt/homebrew/opt/glfw/include
-L/opt/homebrew/opt/glew/lib \
-L/opt/homebrew/opt/glfw/lib \

Then run:
``` bash
make
```

## Testing and Running
#### Running the Main Program
``` bash
./HelloTriangle
```
Controls:
- Left arrow key : translate left along x axis
- Right arrow key : translate right along x axis
- Up arrow key : translate up  along y axis
- Down arrow key : translate down along y axis
- C key : translate forward along z axis
- V key : translate backward along z axis
- A key : rotate clockwise around y axis
- D key : rotate counterclockwise around y axis
- S key : rotate clockwise around x axis
- W key : rotate counterclockwise around x axis
- Q key : rotate clockwise around z axis
- E key : rotate counterclockwise around z axis
- Z key : scale up
- X key : scale down
- Space bar : toggle between CPU and GPU transformations

#### Running Part 5
``` bash
./part5
```
Controls:
- Space bar : toggle rotation around axis connecting the center of both objects

## Assumptipons
This project was developed on a mac and is intended to run in a Unix-like environment.


