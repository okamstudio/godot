## Primitives:

- `prim.c` contains Windows primitives for OS allocation.

## Event Tracing for Windows (ETW)

- `etw.h` is generated from `etw.man` which contains the manifest for mimalloc events.
  (100 is an allocation, 101 is for a free)

- `etw-mimalloc.wprp` is a profile for the Windows Performance Recorder (WPR).
  In an admin prompt, you can use:
  ```
  > wpr -start src\prim\windows\etw-mimalloc.wprp -filemode
  > <my mimalloc program>
  > wpr -stop test.etl
  ``` 
  and then open `test.etl` in the Windows Performance Analyzer (WPA).