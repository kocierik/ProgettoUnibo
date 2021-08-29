#include "drawWindow.hpp"
#include "engineGame.hpp"

#include <ncurses.h>

#include <cmath>
#include <ctime>
#include <iostream>

// Numero di casi dello switch che gestisce i bonus. Equivale a: n bonus
#define BONUS 15

const int scoreForKill = 300;
float finalScore = 0;

EngineGame::EngineGame(int frameGameX, int frameGameY, int topHeigth, int bottomHeigth, int leftWidth, int rightWidth) {
  this->frameGameX = frameGameX;
  this->frameGameY = frameGameY;
  this->topHeigth = topHeigth;
  this->bottomHeigth = bottomHeigth;
  this->leftWidth = leftWidth;
  this->rightWidth = rightWidth;
  this->playerBullets = NULL;
  this->normalEnemyBullets = NULL;
  this->specialEnemyBullets = NULL;
  this->bossEnemyBullets = NULL;
  this->quit = false;
  this->pause = true;
  this->whileCount = 0;
  this->whileCountEnemy = 0;
}

void EngineGame::baseCommand() {
  initscr();
  cbreak();
  curs_set(0);
  nodelay(stdscr, TRUE);
  keypad(stdscr, true);
  curs_set(FALSE);
  noecho();
  use_default_colors();
  start_color();
}

Pbullet EngineGame::generateBullets(Character character, bool isPlayerBullet,
                                    bool moveFoward, Pbullet &bulletList) {
  Pbullet bullet = new Bullet;
  bullet->x = character.getX();
  bullet->y = character.getY();
  bullet->speed = 1;
  bullet->skin = character.getGun().getBulletSkin();
  bullet->isPlayerBullet = isPlayerBullet;
  bullet->moveFoward = moveFoward;
  bullet->next = bulletList;
  return bullet;
}

void EngineGame::generateEnemyBullets(pEnemyList enemyList, Pbullet &enemyBulletList,
                                      Character character) {
  bool shootFoward;
  while (enemyList != NULL) {
    if (this->whileCountEnemy % 20 == 0) {
      shootFoward = true;
      // Se il player è alla sx del nemico, spara a sx
      if (character.getX() > enemyList->enemy.getX())
        shootFoward = false;
      // Colpo del nemico -> false; Sparo avanti/indieto -> moveFoward
      enemyBulletList = generateBullets(enemyList->enemy, false, shootFoward, enemyBulletList);
    }
    enemyList = enemyList->next;
  }
}

void EngineGame::moveBullets(Pbullet bulletList) {
  char tmpSkin[2];
  while (bulletList != NULL) {
    if ((bulletList->isPlayerBullet && bulletList->moveFoward) ||
        (!bulletList->isPlayerBullet && !bulletList->moveFoward))
      bulletList->x += bulletList->speed;
    else
      bulletList->x -= bulletList->speed;

    move(bulletList->y, bulletList->x);
    tmpSkin[0] = bulletList->skin;
    if (bulletList->isPlayerBullet) {
      init_pair(10, COLOR_YELLOW, -1);  // SPARA BANANE GIALLE
      attron(COLOR_PAIR(10));
      printw(tmpSkin);
      attroff(COLOR_PAIR(10));
    } else
      printw(tmpSkin);
    bulletList = bulletList->next;
  }
}

void EngineGame::destroyBullet(Pbullet &bulletList, int xP) {
  if (xP == this->leftWidth || xP == this->rightWidth)
    bulletList = NULL;
  else {
    Pbullet head = bulletList, prev = bulletList, tmp;
    bool mustDestroyCondition = false;
    int range;
    while (head != NULL) {
      range = -1;
      if ((head->isPlayerBullet && head->moveFoward) ||
          (!head->isPlayerBullet && !head->moveFoward))
        range = 1;

      mustDestroyCondition = !isEmpty(head->x + range, head->y) &&
                            !isBonus(head->x + range, head->y) && !isBullet(head->x + range, head->y);
      if (!head->isPlayerBullet)
        mustDestroyCondition &= !isEnemy(head->x + range, head->y);

      if (mustDestroyCondition || head->x > 70 || head->x < 23) {
        if (head == bulletList) {
          tmp = bulletList;
          bulletList = head->next;
          delete tmp;
          prev = bulletList;
          head = bulletList;
        } else {
          tmp = prev->next;
          prev->next = head->next;
          delete tmp;
          head = prev->next;
        }
      } else {
        prev = head;
        head = head->next;
      }
    }
  }
}

pEnemyList EngineGame::destroyEnemy(pEnemyList enemyList, Enemy enemy) {
  pEnemyList head = enemyList, prev = enemyList, tmp;
  char tmpSkin[2];
  while (enemyList != NULL) {
    if (enemyList->enemy.getX() == enemy.getX() &&
        enemyList->enemy.getY() == enemy.getY()) {
      tmpSkin[0] = enemyList->enemy.getSkin();
      mvprintw(enemyList->enemy.getY(), enemyList->enemy.getX(), tmpSkin);
      if (enemyList == head) {
        tmp = head;
        head = enemyList->next;
        delete tmp;
        prev = head;
        enemyList = head;
      } else {
        tmp = prev->next;
        prev->next = enemyList->next;
        delete tmp;
        enemyList = prev->next;
      }
    } else {
      prev = enemyList;
      enemyList = enemyList->next;
    }
  }
  return head;
}

pPosition EngineGame::deletePosition(pPosition positionList, pPosition toDelete) {
  /**
   * Essendo bonus e montagne la stessa tipologia di dato (pPosition), questa
   * funzione elimina un elemento (toDelete) da una lista, che sia un bonus o
   * una montagna.
   */
  pPosition head = positionList, prev = positionList, tmp;
  while (positionList != NULL) {
    if (positionList->x == toDelete->x && positionList->y == toDelete->y) {
      if (positionList == head) {
        tmp = head;
        head = positionList->next;
        delete tmp;
        prev = head;
        positionList = head;
      } else {
        tmp = prev->next;
        prev->next = positionList->next;
        delete tmp;
        positionList = prev->next;
      }
    } else {
      prev = positionList;
      positionList = positionList->next;
    }
  }
  return head;
}

bool EngineGame::checkNoEnemy(DrawWindow drawWindow, pEnemyList enemyList[]) {
  int length = 0;
  for (int i = 0; i < 3; i++)
    length += drawWindow.lengthEnemyList(enemyList[i]);
  return length == 0;
}

/**
 * Funzione per la collisione fisica tra player e nemici.
 * Controlla la presenza di uno scontro in qualsiasi direzione,
 * danneggia 1 il player, 3 il nemico (soluzione kamikaze efficace),
 * e colora di rosso i protagonisti dello scontro.
 */
void EngineGame::checkEnemyCollision(Character &character,
                                     pEnemyList enemyList) {
  pEnemyList tmp = enemyList;
  char tmpSkin[2];
  int xP = character.getX(), yP = character.getY();
  int xE, yE;
  while (enemyList != NULL) {
    xE = enemyList->enemy.getX(), yE = enemyList->enemy.getY();

    if ((xP == xE && yP + 1 == yE) || (xP == xE && yP - 1 == yE)) {
      character.decreaseLife(1);
      enemyList->enemy.decreaseLife(2);

      if (enemyList->enemy.getLife() <= 0)
        enemyList = destroyEnemy(tmp, enemyList->enemy);

      init_pair(13, COLOR_RED, -1);
      attron(COLOR_PAIR(13));
      tmpSkin[0] = character.getSkin();
      mvprintw(character.getY(), character.getX(), tmpSkin);
      tmpSkin[0] = enemyList->enemy.getSkin();
      mvprintw(enemyList->enemy.getY(), enemyList->enemy.getX(), tmpSkin);
      attroff(COLOR_PAIR(13));
    }
    enemyList = enemyList->next;
  }
}

void EngineGame::checkBulletCollision(Pbullet &bulletList, Character &character,
                                      pEnemyList enemyList, int &pointOnScreen,
                                      bool immortalityCheck) {
  bool enemyHit = false, characterHit = false, pause = false;
  Pbullet head = bulletList;
  pEnemyList tmp = enemyList;
  int xE, yE, xP, yP;
  while (enemyList != NULL && !enemyHit && !characterHit) {
    while (bulletList != NULL && !enemyHit && !characterHit) {
      if (bulletList->isPlayerBullet) {
        xE = enemyList->enemy.getX(), yE = enemyList->enemy.getY();
        if ((xE == bulletList->x + 1 && yE == bulletList->y) ||
            (xE == bulletList->x - 1 && yE == bulletList->y))
          enemyHit = true;
      } else {
        xP = character.getX(), yP = character.getY();
        if ((xP == bulletList->x + 1 && yP == bulletList->y) ||
            (xP == bulletList->x - 1 && yP == bulletList->y))
          characterHit = true;
      }
      bulletList = bulletList->next;
    }
    bulletList = head;
    if (enemyHit || characterHit)
      break;
    else
      enemyList = enemyList->next;
  }

  init_pair(13, COLOR_RED, -1);
  attron(COLOR_PAIR(13));
  char tmpSkin[2];
  if (enemyHit) {
    tmpSkin[0] = enemyList->enemy.getSkin();
    mvprintw(enemyList->enemy.getY(), enemyList->enemy.getX(), tmpSkin);
    enemyList->enemy.decreaseLife(character.getGun().getDamage());
    if (enemyList->enemy.getLife() <= 0) {
      enemyList = destroyEnemy(tmp, enemyList->enemy);
      increasePointOnScreen(pointOnScreen, scoreForKill);
    }
  } else if (characterHit && immortalityCheck == false) {
    character.decreaseLife(enemyList->enemy.getGun().getDamage());
    checkDeath(pause, character);
    tmpSkin[0] = character.getSkin();
    mvprintw(character.getY(), character.getX(), tmpSkin);
  }
  attroff(COLOR_PAIR(13));
}

bool EngineGame::isEmpty(int x, int y) { return mvinch(y, x) == ' '; }
bool EngineGame::isBonus(int x, int y) { return mvinch(y, x) == '?'; }
bool EngineGame::isMountain(int x, int y) { return mvinch(y, x) == '^'; }
bool EngineGame::isPlayer(int x, int y) { return mvinch(y, x) == 'M'; }
bool EngineGame::isEnemy(int x, int y) {
  return mvinch(y, x) == 'e' || mvinch(y, x) == 'E' || mvinch(y, x) == 'B';
}
bool EngineGame::isBullet(int x, int y) {
  return mvinch(y, x) == '~' || mvinch(y, x) == '-' || mvinch(y, x) == '=' ||
         mvinch(y, x) == '*';
}
bool EngineGame::isPlayerBullet(int x, int y) { return mvinch(y, x) == '~'; }
bool EngineGame::isEnemyBullet(int x, int y) {
  return mvinch(y, x) == '-' || mvinch(y, x) == '=' || mvinch(y, x) == '*';
}

void EngineGame::moveCharacter(
    DrawWindow drawWindow, Character &character, int direction, pRoom &roomList,
    pEnemyList normalEnemyList, int &pointsOnScreen, int &bananas,
    int &powerUpDMG, bool &bonusPicked, int &bonusType, int &bonusTime,
    bool &upgradeBuyed, int &upgradeType, int &upgradeTime,
    bool &immortalityCheck, int &immortalityTime, bool &toTheRight, int upgradeCost) {
  srand(time(0));
  switch (direction) {  // Movimento con wasd per il player 1 e frecce per il player 2
    case 'W':
    case 'w':
      if (isEmpty(character.getX(), character.getY() - 1))
        character.directionUp();
      else if (isBonus(character.getX(), character.getY() - 1)) {
        bonusTime = 0;       // RESETTA IL TEMPO DI APPARIZIONE SE IL TIMER
                             // ERA GIA ATTIVO PER IL PRECEDENTE BONUS.
        bonusPicked = true;  // FLAG CHE INDICA SE È STATO RACCOLTO
        bonusType = rand() % BONUS;  // 0 <= bonusType < BONUS
        roomList->bonusList = getBonus(
            drawWindow, character.getX(), character.getY() - 1,
            roomList->next->bonusList, normalEnemyList, pointsOnScreen,
            character, bonusType, immortalityCheck, immortalityTime);
        character.directionUp();
      }
      break;
    case 'S':
    case 's':
      if (isEmpty(character.getX(), character.getY() + 1))
        character.directionDown();
      else if (isBonus(character.getX(), character.getY() + 1)) {
        bonusTime = 0;
        bonusPicked = true;
        bonusType = rand() % BONUS;
        roomList->bonusList = getBonus(
            drawWindow, character.getX(), character.getY() + 1,
            roomList->next->bonusList, normalEnemyList, pointsOnScreen,
            character, bonusType, immortalityCheck, immortalityTime);
        character.directionDown();
      }
      break;
    case 'A':
    case 'a':
      if (isEmpty(character.getX() - 1, character.getY()))
        character.directionLeft();
      else if (isBonus(character.getX() - 1, character.getY())) {
        bonusTime = 0;
        bonusPicked = true;
        bonusType = rand() % BONUS;  // GENERA IL TIPO DI BONUS.
        roomList->bonusList = getBonus(
            drawWindow, character.getX() - 1, character.getY(),
            roomList->next->bonusList, normalEnemyList, pointsOnScreen,
            character, bonusType, immortalityCheck, immortalityTime);
        character.directionLeft();
      }
      toTheRight = false;
      break;
    case 'D':
    case 'd':
      if (isEmpty(character.getX() + 1, character.getY()))
        character.directionRight();
      else if (isBonus(character.getX() + 1, character.getY())) {
        bonusTime = 0;
        bonusPicked = true;
        bonusType = rand() % BONUS;
        roomList->bonusList = getBonus(
            drawWindow, character.getX() + 1, character.getY(),
            roomList->next->bonusList, normalEnemyList, pointsOnScreen,
            character, bonusType, immortalityCheck, immortalityTime);
        character.directionRight();
      }
      toTheRight = true;
      break;
    case 'G':  // Sparo in avanti del player
    case 'g':
      if (whileCount / 2 > 1 && character.getGun().getMagazineAmmo() > 0) {
        character.decreaseMagazineAmmo(1);
        this->playerBullets =
            generateBullets(character, true, true, this->playerBullets);
        whileCount = 0;
      }
      break;
    case 'F':  // Sparo all'indietro del player
    case 'f':
      if (whileCount / 2 > 1 && character.getGun().getMagazineAmmo() > 0) {
        character.decreaseMagazineAmmo(1);
        this->playerBullets =
            generateBullets(character, true, false, this->playerBullets);
        whileCount = 0;
      }
      break;
    case 'R':
    case 'r':
      if (character.getGun().getMagazineAmmo() >= 0 &&
          character.getGun().getMagazineAmmo() <
              character.getGun().getMagazineCapacity() &&
          character.getGun().getTotalAmmo() > 0)
        character.reload();
      break;
    case 'Z':  // CONTROLLA L'AQUISTO DI VITE, MASSIMO 3 -------------------
    case 'z':
      if (bananas >= (upgradeCost/2) && character.getNumberLife() < 3) {
        upgradeBuyed = true;  // INDICA CHE È STATO COMPRATO UN UPGRADE
        upgradeType = 0;      // INDICA IL TIPO DI UPGRADE.
        upgradeTime = 0;  // RESETTA IL TEMPO DI APPARIZIONE SE HAI COMPRATO UN
                          // ALTRO UPGRADE
        character.setNumberLife(character.getNumberLife() + 1);
        bananas = bananas - (upgradeCost/2);
      }
      break;
    case 'X':  // CONTROLLA L'AQUISTO DI POWERUP AL DANNO, SONO ACQUISTABILI AL
               // MASSIMO 4 DURANTE TUTTA LA RUN
    case 'x':
      if (character.getGun().getDamage() < 50) {
        if (bananas >= upgradeCost && powerUpDMG < 3) {
          upgradeBuyed = true;
          upgradeType = 1;
          upgradeTime = 0;
          character.increaseDamageGun(5);
          if (character.getGun().getDamage() >= 50) character.setGunDamage(50);
          bananas = bananas - upgradeCost;
          powerUpDMG++;
        } else if (bananas >= upgradeCost && (powerUpDMG == 3)) {
            upgradeBuyed = true;
            upgradeType = 1;
            upgradeTime = 0;
            character.increaseDamageGun(10);
            if (character.getGun().getDamage() >= 50) character.setGunDamage(50);
            bananas = bananas - upgradeCost;
            powerUpDMG++;
          }
        // MESSAGGIO CHE SEGNALE IL FATTO CHE QUESTO UPGRADE NON è DISPONIBILE
        //mvprintw(?, ?, "MAX DAMAGE OBTAINED");
      }
      break;
  }
}

void EngineGame::gorillaPunch(int direction, Character &character,
                              pEnemyList enemyList, int &pointOnScreen,
                              bool toTheRight) {
  pEnemyList tmp = enemyList;
  if (direction == 32) {
    int range = -1;
    if (toTheRight) range = 1;
    // Con questa condizione, l'animazione del danno è più chiara
    if (isEmpty(character.getY(), character.getX() + range))
      mvprintw(character.getY(), character.getX() + range, "o");

    while (enemyList != NULL) {
      if (character.getX() + range == enemyList->enemy.getX() &&
          character.getY() == enemyList->enemy.getY()) {
        // Nemico rosso quando riceve il punch
        init_pair(13, COLOR_RED, -1);
        attron(COLOR_PAIR(13));
        char tmpSkin[0];
        tmpSkin[0] = enemyList->enemy.getSkin();
        mvprintw(enemyList->enemy.getY(), enemyList->enemy.getX(), tmpSkin);

        enemyList->enemy.decreaseLife(40);
        if (enemyList->enemy.getLife() <= 0) {
          enemyList = destroyEnemy(tmp, enemyList->enemy);
          increasePointOnScreen(pointOnScreen, scoreForKill);
        }
        attroff(COLOR_PAIR(13));
      }
      enemyList = enemyList->next;
    }
  }
}

void EngineGame::choiceGame(DrawWindow drawWindow, int *direction,
                            int *selection) {
  int cnt = 0;
  while (*direction != 32) {
    drawWindow.drawMenu();
    drawWindow.printCommand(&cnt);
    *direction = getch();
    if (*direction == 32) *selection = cnt;
    if (*direction == KEY_UP) cnt--;
    if (*direction == KEY_DOWN) cnt++;
    if (cnt > 5) cnt = 0;
    if (cnt < 0) cnt = 5;
  }
}

pEnemyList EngineGame::generateEnemy(int *enemyCount, int type, pEnemyList enemyList,
                                     DrawWindow drawWindow) {
  // Variables 4 basic enemies
  char skin = 'e';
  int life = 10;
  Gun gun('-', 10, -1, -1);
  switch (type) {
    case 0:  // Basic enemies, no variables changes
      break;
    case 1:  // Elite enemies
      skin = 'E';
      life = 10;
      gun.setBulletSkin('=');
      gun.setDamage(15);
      break;
    case 2:  // Boss enemy
      skin = 'B';
      life = 10;
      gun.setBulletSkin('*');
      gun.setDamage(25);
      break;
  }

  bool isEmpty = false;
  int x, y;
  while (*enemyCount > 0) {
    x = drawWindow.randomPosition(40, 69).x;
    y = drawWindow.randomPosition(8, 19).y; 
    pEnemyList head = new EnemyList;
    Enemy enemy(x, y, skin, life, 1, gun);
    head->enemy = enemy;
    head->next = enemyList;
    *enemyCount -= 1;
    enemyList = head;
    isEmpty = true;
  }
  if (isEmpty) {
    pEnemyList head = new EnemyList;
    Enemy enemy(0, 0, ' ', life, 1, gun);
    head->enemy = enemy;
    head->next = enemyList;
    enemyList = head;
    isEmpty = false;
  }
  return enemyList;
}

pPosition EngineGame::getBonus(DrawWindow drawWindow, int x, int y,
                               pPosition bonusList, pEnemyList &enemyList,
                               int &pointsOnScreen, Character &character,
                               int &bonusType, bool &immortalitycheck,
                               int &immortalityTime) {
  pPosition tmpHead = bonusList;
  bool end;
  while (bonusList->next != NULL) {
    if (bonusList->x == x && bonusList->y == y && bonusList->skin == '?') {
      end = false;
      switch (bonusType) {
        case 0:  // Bonus name: "BUNCH OF BANANAS"
          pointsOnScreen += 50;
          end = true;
          break;
        case 1:  // Bonus name: "CRATE OF BANANAS"
          pointsOnScreen += 300;
          end = true;
          break;
        case 2:  // Bonus name: "SUPPLY OF BANANAS"
          pointsOnScreen += 1000;
          end = true;
          break;
        case 3:  // Malus name: "ROTTEN BANANAS"
          pointsOnScreen -= 100;
          end = true;
          break;
        case 4:  // Malus name: "BANANAS SPIDER [-10 HP]"
          if (character.getNumberLife() == 1 && character.getLife() <= 10)
            character.setLife(5);
          else
            character.decreaseLife(10);
          end = true;
          break;
        case 5:  // Malus name: "MONKEY TRAP [-30 HP]"
          if (character.getNumberLife() == 1 && character.getLife() <= 30)
            character.setLife(5);
          else
            character.decreaseLife(30);
          end = true;
          break;
        case 6:  // Bonus name: "EAT 1 BANANA [+10 HP]"
          character.increaseLife(10);
          end = true;
          break;
        case 7:  // Bonus name: "EAT 2 BANANA [+20 HP]"
          character.increaseLife(20);
          end = true;
          break;
        case 8:  // Bonus name: "BANANA SMOOTHIE [+40 HP]"
          character.increaseLife(40);
          end = true;
          break;
        case 9:  // Bonus name: "PEEL LOADER [+20 PEELS]"
          character.increaseTotalAmmo(20);
          end = true;
          break;
        case 10:  // Bonus name: "PEEL BOX [+40 PEELS]"
          character.increaseTotalAmmo(40);
          end = true;
          break;
        case 11:  // Bonus name: "PEACE MISSION [+100 PEELS]"
          character.increaseTotalAmmo(100);
          end = true;
          break;
        case 12:  // Malus name: "PISSED OFF MONKEYS"
          while (enemyList != NULL) {
            enemyList->enemy.increaseLife(25);  // Aumenta la vita dei nemici
            Gun tmpBetterGun = enemyList->enemy.getGun();
            tmpBetterGun.increaseDamage(10);  // Aumenta il danno dei nemici
            enemyList->enemy.setGun(tmpBetterGun);
            enemyList = enemyList->next;
          }
          end = true;
          break;
        case 13:  // Bonus name: "PEELS ON FIRE! [+5 DAMAGE]"
          if (character.getGun().getDamage() < 50) {
            Gun tmpBetterGun = character.getGun();
            tmpBetterGun.increaseDamage(5);
            character.setGun(tmpBetterGun);
          } else
            character.getGun().increaseTotalAmmo(30);
          end = true;
          break;
        case 14:  // Bonus name: "MONKEY GOD! [IMMORTALITY]"
          immortalityTime = 0;
          immortalitycheck = true;
          end = true;
          break;
          /*
         case n:     // Malus name: "BANANA FRAGRANCE" // Genera n nemici
             int tmpQuantity = 3, tmpRound = round;
             enemyList = generateNormalEnemy (&tmpQuantity, 'X', 10, 100,
         enemyList, tmpRound, drawWindow); drawWindow.printEnemy (enemyList,
         drawWindow); end = true; break;
         */
      }
      if (end) {
        bonusList = deletePosition(tmpHead, bonusList);
        return bonusList;
      }
    }
    bonusList = bonusList->next;
  }
  return tmpHead;
}

void EngineGame::checkDeath(bool &pause, Character &character) {
  if (character.getLife() <= 0) {
    character.setNumberLife(character.getNumberLife() - 1);
    if (character.getNumberLife() > 0) {
      character.setLife(100);
    }
  }
  if (character.getNumberLife() <= 0) {
    pause = true;
  }
}

void EngineGame::checkMountainDamage(Pbullet bulletList,
                                     pPosition &mountainList) {
  pPosition tmpMountainList = mountainList;
  int range;
  while (bulletList != NULL) {
    range = -2;
    if ((bulletList->isPlayerBullet && bulletList->moveFoward) ||
        (!bulletList->isPlayerBullet && !bulletList->moveFoward))
      range = 2;

    while (tmpMountainList != NULL) {
      if (bulletList->x + range == tmpMountainList->x &&
          bulletList->y == tmpMountainList->y) {
        tmpMountainList->life -= 1;
        if (tmpMountainList->life <= 0)
          mountainList = deletePosition(mountainList, tmpMountainList);
      }
      tmpMountainList = tmpMountainList->next;
    }
    tmpMountainList = mountainList;

    bulletList = bulletList->next;
  }
}

void EngineGame::engine(Character character, DrawWindow drawWindow) {
  int direction, selection;
  baseCommand();
  choiceGame(drawWindow, &direction, &selection);
  while (pause) {
    switch (selection) {
      case 0:     // FUN (single player)
        pause = false;
        runGame(character, drawWindow, direction);
        clear();
        drawWindow.loseScreen(direction, finalScore);
        selection = 5;
        break;
      case 1:     // MORE FUN (multiplayer)
        /*
        pause = false;
        runGame(character, drawWindow, direction);
        clear();
        drawWindow.loseScreen(direction, finalScore);
        selection = 5;
        break;
        */
      case 2:     // HOW TO PLAY
        clear();
        drawWindow.HowToPlay(direction);
        selection = 5;
        break;
      case 3:     // LEADERBOARD
        clear();
        drawWindow.leaderboardScreen(direction);
        selection = 5;
        break;
      case 4:     // CREDITS
        clear();
        drawWindow.credits(direction);
        selection = 5;
        break;
      case 5:     // QUIT!
        refresh();
        std::cout << "Thanks for playing ;) ";
        exit(1);
        break;
      case 6:     // ???
        clear();
        engine(character, drawWindow);
        break;
    }
  }
  endwin();
}

void EngineGame::increaseCount(int &whileCount, long &points) {
  whileCount += 1;
  points += 1;
  this->whileCountEnemy += 1;
}

void EngineGame::money(int &bananas, bool noEnemy, int maxRound, int &roundPayed, Character &character, int upgradeCost) {
  srand(time(NULL));
  if (noEnemy && (maxRound != roundPayed)) {  // CONTROLLA CHE LA STANZA SIA PULITA E CHE L'ULTIMO ROUND SIA STATO PAGATO
    bananas = bananas + rand() % 3 + 1;
    character.increaseLife((rand() % 20 + 20));
    if (maxRound >= 2 && maxRound <= 5)
      character.increaseTotalAmmo(30);
    else if (maxRound > 5 && maxRound <= 8)
      character.increaseTotalAmmo(40);
    else if (maxRound > 8 && maxRound <= 12)
      character.increaseTotalAmmo(50);
    else if (maxRound > 15)
      character.increaseTotalAmmo(80);
    roundPayed++;
  }
  init_pair(20, COLOR_GREEN, -1);
  attron(COLOR_PAIR(20));
  if (bananas >= (upgradeCost))
    mvprintw(24, 52, "LIFE OR DAMAGE PURCHASABLE!");
  else if(bananas >= (upgradeCost/2))
    mvprintw(24, 52, "EXTRA LIFE PURCHASABLE!"); 
  attroff(COLOR_PAIR(20));
}

void EngineGame::printList(pPosition positionList) {
  int i = 2;
  while (positionList != NULL) {
    mvprintw(i, 0, "Vita montagna  %d", positionList->life);
    positionList = positionList->next;
    i++;
  }
}

void EngineGame::getInput(int &direction) { direction = getch(); }

void EngineGame::isPause(int &direction, bool &pause) {
  if (direction == 27) pause = true;
}

void EngineGame::increasePointOnScreen(int &pointOnScreen, int pointsAdded) {
  pointOnScreen += pointsAdded;
}

void EngineGame::runGame(Character character, DrawWindow drawWindow,
                         int direction) {
  bool toTheRight = true;
  bool upgradeBuyed = false, bonusPicked = false, immortalityCheck = false;
  int upgradeCost = 10;
  bool noEnemy = false;
  int immortalityTime = 0;
  int pointsOnScreen = 0;
  long points = 0;
  int powerUpDMG = 0;  // NUMERO DI POWERUP AL DANNO AQUISTATI
  int bananas = 0, roundPayed = 0;
  int bonusTime = 0, upgradeTime = 0, bonusType = 0, upgradeType = 0;
  int maxRound = 1;
  int normalEnemyCount = 1, specialEnemyCount = 0, bossEnemyCount = 0;
  int enemyCounters[2];
  enemyCounters[0] = 1;
  enemyCounters[1] = 0;
  enemyCounters[2] = 0;

  pPosition mountainList = new Position, bonusList = new Position;
  pRoom roomList = new Room;

  pEnemyList enemyLists[2];
  enemyLists[0] = NULL;
  enemyLists[1] = NULL;
  enemyLists[2] = NULL;

  Gun basicPlayerGun('~', 25, 40, 10);
  character.setGun(basicPlayerGun);
  clear();
  while (!pause) {
    roomList = drawWindow.changeRoom(character, normalEnemyCount, specialEnemyCount, bossEnemyCount, enemyLists, mountainList, bonusList, roomList, maxRound);
    enemyLists[0] = generateEnemy(&normalEnemyCount, 0, enemyLists[0], drawWindow);
    enemyLists[1] = generateEnemy(&specialEnemyCount, 1, enemyLists[1], drawWindow);
    enemyLists[2] = generateEnemy(&bossEnemyCount, 2, enemyLists[2], drawWindow);

    getInput(direction);
    moveCharacter(drawWindow, character, direction, roomList, enemyLists[0],
                  pointsOnScreen, bananas, powerUpDMG, bonusPicked, bonusType,
                  bonusTime, upgradeBuyed, upgradeType, upgradeTime, immortalityCheck,
                  immortalityTime, toTheRight, upgradeCost);
    clear();

    noEnemy = checkNoEnemy(drawWindow, enemyLists);
    
    drawWindow.printCharacter(character.getX(), character.getY(),
                              character.getSkin());
    drawWindow.drawRect(this->frameGameX, this->frameGameY, this->rightWidth,
                        this->bottomHeigth, noEnemy, maxRound, false);
    drawWindow.drawStats(this->frameGameX, this->frameGameY, this->rightWidth,
                         this->bottomHeigth, pointsOnScreen, character,
                         noEnemy, powerUpDMG, bananas, maxRound, roomList);
    drawWindow.drawLeaderboardOnScreen();
    drawWindow.printCharacterStats(enemyLists[0], enemyLists[1], enemyLists[2], character);

    if (drawWindow.lengthListRoom(roomList) > 1) {
      drawWindow.printMountain(roomList->next->mountainList);
      drawWindow.printBonus(roomList->next->bonusList);
      checkMountainDamage(this->playerBullets, roomList->next->mountainList);
      checkMountainDamage(this->normalEnemyBullets,
                          roomList->next->mountainList);
      checkMountainDamage(this->specialEnemyBullets,
                          roomList->next->mountainList);
      checkMountainDamage(this->bossEnemyBullets,
                          roomList->next->mountainList);
    }

    increaseCount(this->whileCount, points);
    /**
     * Temporanea soluzione affinché point non raggiunga valori esorbitanti.
     * In futuro verrà resettata quando verranno uccisi tutti i nemici e si potrà
     * dunque accedere alle altre stanze.
    */
    if (points > 500) points = 0;

    //drawWindow.printEnemy(enemyLists, drawWindow);
    drawWindow.printEnemy(enemyLists[0], drawWindow);
    drawWindow.printEnemy(enemyLists[1], drawWindow);
    drawWindow.printEnemy(enemyLists[2], drawWindow);

    drawWindow.moveEnemy(enemyLists[0], character, drawWindow, points);
    drawWindow.moveEnemy(enemyLists[1], character, drawWindow, points);
    drawWindow.moveEnemy(enemyLists[2], character, drawWindow, points);

    generateEnemyBullets(enemyLists[0], this->normalEnemyBullets, character);
    generateEnemyBullets(enemyLists[1], this->specialEnemyBullets, character);
    generateEnemyBullets(enemyLists[2], this->bossEnemyBullets, character);

    moveBullets(this->playerBullets);
    moveBullets(this->normalEnemyBullets);
    moveBullets(this->specialEnemyBullets);
    moveBullets(this->bossEnemyBullets);

    checkEnemyCollision(character, enemyLists[0]);
    checkEnemyCollision(character, enemyLists[1]);
    checkEnemyCollision(character, enemyLists[2]);

    gorillaPunch(direction, character, enemyLists[0], pointsOnScreen,
                 toTheRight);
    gorillaPunch(direction, character, enemyLists[1], pointsOnScreen,
                 toTheRight);
    gorillaPunch(direction, character, enemyLists[2], pointsOnScreen,
                 toTheRight);

    money(bananas, noEnemy, maxRound, roundPayed, character, upgradeCost);
    checkBulletCollision(this->playerBullets, character, enemyLists[0],
                         pointsOnScreen, immortalityCheck);
    checkBulletCollision(this->playerBullets, character, enemyLists[1],
                         pointsOnScreen, immortalityCheck);
    checkBulletCollision(this->playerBullets, character, enemyLists[2],
                         pointsOnScreen, immortalityCheck);
    checkBulletCollision(this->normalEnemyBullets, character, enemyLists[0],
                         pointsOnScreen, immortalityCheck);
    checkBulletCollision(this->specialEnemyBullets, character, enemyLists[1],
                         pointsOnScreen, immortalityCheck);
    checkBulletCollision(this->bossEnemyBullets, character, enemyLists[2],
                         pointsOnScreen, immortalityCheck);
    
    refresh();
  
    destroyBullet(this->playerBullets, character.getX());
    destroyBullet(this->normalEnemyBullets, character.getX());
    destroyBullet(this->specialEnemyBullets, character.getX());
    destroyBullet(this->bossEnemyBullets, character.getX());
    drawWindow.showBonusOnScreen(upgradeBuyed, upgradeType, upgradeTime,
                                 bonusPicked, bonusType, bonusTime,
                                 immortalityCheck, immortalityTime);
    checkDeath(pause, character);

    timeout(50);
    isPause(direction, pause);
    finalScore = pointsOnScreen;
  }
}
// x = 23 | y = 8 HL | end | x = 70 | y = 19 | RD
