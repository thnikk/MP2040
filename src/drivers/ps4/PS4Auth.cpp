/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 * Trimmed for MP2040: USB auth-dongle passthrough only.
 */

#include "drivers/ps4/PS4Auth.h"
#include "drivers/ps4/PS4AuthUSBListener.h"

void PS4Auth::initialize() {
    ps4AuthData.passthrough_state = GPAuthState::auth_idle_state;
    ps4AuthData.dongle_ready = false;

    PS4AuthUSBListener * ps4listener = new PS4AuthUSBListener();
    ps4listener->setAuthData(&ps4AuthData);
    listener = ps4listener;
}

// Only available on boards with a USB host port wired up (see BoardConfig.h
// USB_HOST_PIN_DP and usbhostmanager.cpp).
bool PS4Auth::available() {
#ifdef USB_HOST_PIN_DP
    return true;
#else
    return false;
#endif
}

void PS4Auth::process() {
    ((PS4AuthUSBListener *)listener)->process();
}

void PS4Auth::resetAuth() {
    ps4AuthData.passthrough_state = GPAuthState::auth_idle_state;
}
