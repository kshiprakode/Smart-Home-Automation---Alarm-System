The system works in the following way :

1. After the door is closed, the events are caught and the system response are -

Motion		Keychain	Security-System response	Reason
False		False		ON				User is not at home so switch the security system, on.
True		True		OFF				User is back home so switch the system ,off
True		False		Intruder alarm			There is a presence of an intruder
False		True		OFF				User is already at home, so no need to ON the security system

2. In this case, we assume that when the security system is OFF and any person enters the home ,he is not detected (since the security system is "OFF" and cannot detect anything).

3. The gateway output file shows all the messages it receives and sends

4. The gateway interface shows when the security system is turned ON or OFF

5. The Motion, Door, Keychain Sensors and Smart Device output files and interface show only the messages it sends. the vector for that is processed and updated and then it sends the message.

6. Vector Structure is [Motion,Keychain, Door, Gateway]

7. Gateway take time to get the data from the sensors and analyse the result and print it.

8
