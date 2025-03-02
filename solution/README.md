# Solution
Multiple approaches can reveal the flag in this challenge. All such strategies rely on manipulating a key enemy in a platformer game. Reaching the end of this game is its own separate challenge for distracting participants. The game lets the player control a figure to parkour across structures. Other characters patrol around said structures to shoot at sight of the player. The end of parkour stage features an enemy that generates armies of these snipers. These armies tend to brunch together in order to obscure the flag. One approach to this challenge is to break these armies apart. The game initialises a level according to multiple configuration files, which are as follows:
* `Abe/tuto.lvl`, which defines the terrain layout of the level. This file describes a matrix of tiles which form this layout,
* `Abe/tuto.gen`, which defines which enemies that appear in the level. This file defines instances of enemies with their respective starting positions and health,
* `Mukki/moldInfo.txt`, which defines the properties that all instances of an enemy have in common.

A participant can use these files to effectively create situations triggering unusual behaviours. A player can create an environment to remove the obstructions hiding the flag. An example of such a environment is placing the army generator on a ledge. This arrangement causes all enemies arising from the army generator to fall down. Here, gravity was the key to removing the obstructions from the flag. Figure 1 shows how this strategy can appear. 
![Screenshot 2025-03-01 172804](https://github.com/user-attachments/assets/625b9ee5-9ef2-488b-a38c-9227f1435c2c)
Figure 1: Revelation of the flag using a ledge placement strategy. This example changed the spawning position of the generator through the `Abe/tuto.gen` file. Here, this position was inside a cliff leading to a pit. The obstruction became an infinite stream of snipers running to their doom.
Alternatively, the participant may realise that the player can become any enemy. The `Abe/tuto.gen` file also defines which enemy type the player adopts. A unique identifier distinguishes every type of enemy. Changing the enemy type identifier for the player's entry can change the player's type. Changing this type to the one of the generator reveals the flag. Figure 2 shows the outcome of this strategy
![Screenshot 2025-03-01 222722](https://github.com/user-attachments/assets/54697386-1798-4663-9de6-94649e8c9286)
Figure 2: Revelation of the flag via modification of the player's characteristics in the `Abe/tuto.gen` file.

