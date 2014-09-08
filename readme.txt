1. advance from voltage level 1 to 10, each time turning on more stripes of
light. Here are the voltage targets: 14,16,18, 19, 20, 21, 22, 22.5, 23.25, 24
2. If the person gets to level 10 and holds it for more than 3 seconds, do
the reward sequence.
3. Reward: bottom to top with 0.1S intervals 3 times, then ALL ON to get rid
of the power and bring it back down so it's ready for the next pedaler.
4. We will use a difficulty knob. The Voltish concept worked well in the
past. All the way left means a 30% reduction in voltage targets. I need to
know how to wire it.
5. We will possibly need to implement some type of averaging or hysteresis
so it someone is out of the saddle pedaling, the lights don't pulse to their
pedaling.
* if the voltage doesn't increase for 30 seconds, turn on the halogens to
  drain the patient to 12v

Situation is either:
IDLING
PLAYING
FAILING
(LEVEL 10)
VICTORY DANCE
DANCE OVER
HARD RESET

IDLING only goes to PLAYING if volts increases by a real amount indicating pedaling

PLAYING goes to FAILING if voltage keeps falling for at least 30 seconds

PLAYING goes to VICTORY DANCE if volts >= win voltage with all lights on for 3 seconds

FAILING always goes to IDLING before PLAYING. Lift relay after 15 seconds!

VICTORY goes to FAILING after light sequence

You know you're IDLING if volts < 14 and haven't risen and not PLAYING

You know you're PLAYING if volts have risen significantly.

You know you're FAILING if someone set it to FAILING

Ways to improve:
When someone gives up and the knob is easy, the volts falls slowly. then the game never resets. we need 'time since pedaling.'

2014-4-28 emailed pseudocode

Both sLEDgehammers need their own difficulty knob to cover the possibility of doing two events in two places on the same day.

The two boxes also need to be able to 'talk' to each other through a serial cable that I am hoping you can make.

IF the boxes are connected to each other through this cable, follow this pseudocode:

0. Give the boxes names like Box1 and Box2

1. Create a new state called "CLEARLYWINNING". Clearly winning means that you are at least 2 stages ahead of your opponent for more than 2 seconds. So if you are on
level 8 and they are on level 6 or below for more than 2 seconds, you are CLEARLYWINNING.

If Box1 is CLEARLYWINNING, adjust the 'voltish' of Box2, making it easier for them to catch up. This makes the game closer. Start with a 10% adjustment. Reset the
CLEARLYWINNING clock. If CLEARLYWINNING again, do another 10% adjustment.

2. If Box1 = VICTORY, then Box1 enters its victory dance which you will see in the code. Box2 should do a 'failure droop' where it fades out. Note the function
turnThemOffOneAtATime(). This may be useful if for nothing other than to see the timing that I like ( delay of 200ms between levels).  NOTE that the top level STAYS
ON. This is to prevent people from slipping off the pedals when the Halogens turn off and they are still pedaling hard.

3. If Box2 loses a competition because Box1 gets to victory first, then make Box2's state = FAILING. As you'll see in the code, the only way you can get out of
FAILING is for voltage (actual, not adjusted) to fall below 13.5 .
