# Incremental GenAI solution to labs 0 - 3

This repository contains code that was built entirely with GenAI. GenAI was given four prompts. The prompts
were designed to simulate how a non-programmer would instruct GenAI. The prompts were:

1. write a c++ program that plays a simple game of texas holdem in a terminal. The game is simple because there is no betting: just deal out the cards to the players and to the table, as you would in texas holdem and figure out the winner.
1. now add another game to this program - a Pinochle game. Also keep it simple, just deal the cards and figure out the melds
1. you can renames files in my folder
1. add a game of Go Fish to this program, allowing a user to play Go Fish with different decks: pinochle deck, texas holdem deck, or a deck of uno cards

GenAI produced `cardgames.cpp` (file name after prompt 1 was different) and continued adding more code to that file. You can compile and run this code with:
```
g++ -std=c++17 -O2 -Wall -o cardgames cardgames.cpp
./cardgames
``` 
