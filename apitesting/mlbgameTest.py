#!python
from __future__ import print_function
import mlbgame

month = mlbgame.games(2015, 6, home='Mets')
games = mlbgame.combine_games(month)
for game in games:
    print(game)