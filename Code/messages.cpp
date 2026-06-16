/* WarcLock.ino — by Deamonmist (multi-file)
 * Random messages that will scroll across the screen from time to time.
 * Put your own messages between then double quotes!
 *
 */

 #include "messages.h"

 const char* g_messages[] = {
  "Answer ye these questions three!",
  "Zug zug!",
  "Dogmeat looks happy.",
  "To the Mun and back (maybe)!",
  "Deamonmist was here!",
  "Much love to the VallyeTech crew!",
  "Inconceivable!",
  "Remember pogs?",
  "Oh no, not again.",
  "Ah sh*t, here we go again!",
  "I'm 40% Zinc!",
  "I'm 40% Titanium!",
  "Cheese it!",
  "Life! Don't talk to me about life.",
  "Get the freebies!",
  "Time is an illusion.",
  "Last minute panic!",
  "DAYtuh or DAAtuh?",
  "Xiao now, brown cow?",
  "Roll for initiative.",
  "Resistance is futile.",
  "Chevron 7 Locked!",
  "Birds think they are so great...",
  "Cree!",
  "I hear boss music...",
  "When in doubt: cheeseit!",
  "Look out behind you!",
  "Wait, hold on. One second...",
  "Heghlu'meH QaQ jajvam.",
  "Look out in front of you!",
  "Listen to your heart.",
  "Listen to your mind.",
  "This time it'll be awesome, I promise!",
  "You must construct additional pylons!",
  "Witness me!",

 };

 const int g_messageCount = sizeof(g_messages) / sizeof(g_messages[0]);
