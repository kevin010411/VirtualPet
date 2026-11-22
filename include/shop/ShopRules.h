#pragma once

#include "shop/ShopConfirmation.h"

class Pet;

ShopConfirmationNotice shopUnlockNotice(const Pet &pet, const uint8_t *offer);
ShopConfirmationNotice shopPurchaseNotice(const Pet &pet, uint32_t minimumBalance,
                                          const uint8_t *offer, const uint8_t *item,
                                          uint16_t owned, uint32_t balance);
