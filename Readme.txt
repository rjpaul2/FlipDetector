Turning Tracker for Pressure Ulcers
######################################

@date: 03/14/2017
@author: Robert J Paul

@purpose:

Pressure ulcers, or bedsores, is a common skin condition affecting millions of bedridden patients in the US. The condition is caused by the compression of blood capillaries supplying the skin, leading to necrosis, or death, of skin cells.  

Pressure ulcers are a huge problem for many elderly patients, predominantly in low-budget nursing homes. While there are many factors that go into the accumulation of pressure ulcers, one of the main causes is the inaction of nursing assistants (CNAs) or other medical staff to flip their patients regularly. Because ulcers can develop within 2-6 hours (depending on the patient) of stagnant positioning, in the US patients are necessarily to be flipped once every two hours. One of the most simple causes of such high levels of bedsores lies within the organization of nursing staff. Our research shows that beyond all other factors, medical staff tend to become overwhelmed with their other duties, and simply forget to flip their patients, or flip their patients far later than necessary. This is the case, however, with the more ethical of medical staff. There have been known cases of staff purposely missing flips- beith laziness or ignorance. In all cases, though, medical staff are trained to react to �beeping� or �buzzing� which we hope to enact as the standard of patient flipping. 

The following microcontroller code makes a step forward in solving the bedsore �pandemic� - specifically, in lower-budget nursing homes. Our proposed Turning Tracker pad is to be placed underneath respective patients in order to monitor each patient�s two-hour flip timer. The pad consists of multiple force sensors that keep track of the patient's position on the bed. When a significant weight distribution is shifted - i.e. a flip is registered-, the Turning Tracker will reset the two-hour timer on that particular pad. When the timer is depleted, however, a signal is sent to each staff monitoring that particular pad identifying that a patient needs to be flipped. If a  flip is not registered after the required 2-hours, a mechanical buzzer on the device will sound. At which point, the patient would call his or her nurse. 
