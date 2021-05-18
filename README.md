# Collaborative text editor

## RoadMap

1. Intro to cMake;
2. Intro to gRPC;
3. First echo server;
4. Server side operations(Alex);
5. UI(Artem);
6. One cmake for two sides;
7. Connect operations(insert, delete);
8. Crying;
9. Connect operations(undo, redo);
10. More crying;
11. Connect operations(add_line, del_line);
12. Fix bugs and crying;
13. WOW;
14. ---NOW---.

## How to run

```console
git clone ...
cd Collaborative_text_editor
mkdir -p cmake/build
cd cmake/build
cmake ../../service
make(-j)
./server
./client
```
enjoy

## Credits

Frontend end bug fix : Artem Prisyazhnyuk(@artempris);\
Backend and build : Alexandr Shitov(@Atgsan);\
Mentor and reviewer : Danila Kutenin(@Danlark).
