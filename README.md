# Collaborative text editor
This repository contains a realisation of collaborative text editor for second grade of my study in HSE FCS. This realisation based on TreeOPT algorithm(https://members.loria.fr/CIgnat/files/pdf/IgnatCEW02.pdf)

# План:

1. Реализовать простое соединение клиент сервер;
2. Алгоритм;
3. Сделать интерфейс;

## Клиент-сервер

### Технологии:

1. C++
2. gRPC
3. Cmake

### Что нужно продумать

1. Как реализовать связь и причем тут Cmake?
2. Где и как хранить файлы?
3. Как работать с gRPC?

### Текущее состояние

Не понятно, как сделать что-то свое в gRPC. Все что пока сделал, это запустил пару примеров.

## Алгоритм 

### Материалы

1. [Статья про treeOPT](./notes/IgnatCEW02.pdf);
2. [Статья "Concurrency Control in Groupware Systems"](https://www.lri.fr/~mbl/ENS/CSCW/2012/papers/Ellis-SIGMOD89.pdf)
