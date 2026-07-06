---
# Export Options
from: markdown+wikilinks_title_after_pipe+
documentclass: article
geometry:
  - margin=1in
numbersections: false
fontsize: 12pt
mainfont: "Noto Sans"
---

# Floor and Car Controller Design Document

The floor and car controller are both implemented using an `STM32F303RE` and have similar responsibilities/operation. As such, the source code for 
each is the same and can be found in [/FloorController](../FloorController/). 

## Purpose of the Floor Controller

- Respond to button presses on the STM32 daughter board and elevator floor buttons wired via the debounce boards.
- Send floor request upon a floor button press.
- Light LED indicator lights to show that there is a pending request for that floor.

### Extended features

>[!note]
>There exists a subset of messages in the protocol which are "extended" and may break compatibility with other elevator systems implementing the
>same base protocol. They enable extended features.

- Heartbeat response messages (upon heartbeat request from supervisory controller, sends a heartbeat response with the status of the node)
- Receiving floor position messages from the supervisory controller which can be used to disable the LED indicators once the elevator arrives
  indicating the request was fulfilled. May also be used to open doors.

## Purpose of the Car Controller

- Respond to button presses the same as the floor controller
- Light button LED corresponding to current request
- Send floor request message to supervisory controller corresponding to button press

### Extended features

- Heartbeat response messages (see above)
