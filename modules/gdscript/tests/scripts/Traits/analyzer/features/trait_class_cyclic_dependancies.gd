class_name Player
uses PlayerWeapon

enum AttackType { PUNCH, SWORD, SPEAR, HAMMER, BULLET }

class AnotherPlayer:
    uses PlayerWeapon

func test():
    print("ok")
