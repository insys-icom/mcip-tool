# README
"mcip-tool" is a set of tools to use features of the firmware within M3 containers: e.g. get/send SMS, get/set IOs, send CLI commands 

This multi binary contains the tools:

* mcip-tool
* sms-tool
* get-input
* set-output
* get-pulses
* cli-cmd

Get the parameters and help by starting the tool with -h, e.g. "sms-tool -h"

These tools operate within an M3 container. They can only operate when the M3 firmware has allowed them to use the necessary resource like MCIP or access to the CLI without authentication.

## "mcip-tool"
This is a tool to send and receive MCIP messages. This in combination with the "console" program included in MCIP itself is useful for debugging or tests.

## "sms-tool"
Use this tool to send or receive SMS in the container.

To be able to send SMS the container must be configured to allow unauthenticated READ/WRITE access to the CLI. Example for sending an SMS:
<pre>sms-tool -i lte2 -n +49123456789 -t "This is the text"</pre>

To receive SMS the container must be configured to forward SMS to containers. Example for receiving an SMS:
<pre>sms-tool -l</pre>

## "get-input"
Use this tool to get notified when a digital input changes its state.

To be able to get the state change the container must be configured to forward input events to containers. Example for listening for these events:
<pre>get-input -p</pre>

## "set-output"
Use this tool to set the state of a digital output.

To be able to change the state of an output the container must be configured to allow unauthenticated READ/WRITE access to the CLI. Example for setting the digital Output 2.1 to closed:
<pre>set-output -o 2.1 -s close</pre>

## "get-pulses"
Use this tool to get notified when pulses have been detected on a digital input. 

To be able to get the pulses the container must be configured to forward input events to containers. Example for listening for these events:
<pre>get-pulses -p</pre>

## "cli-cmd"
Use this tool to send a command to the routers CLI in order to get or set configuration or get a status value.

To be able to send commands to the CLI the container must be configured to allow unauthenticated access to the CLI. Depending on the granted access rights, the status or the configuration can be read or set. Example for checking the link state of the Ethernet port 1.1:
<pre>cli-cmd status.ethernet1.port[1].link</pre>

Example to change the location setting and activate the new profile:
<pre>cli-cmd administration.hostnames.location=SomePlace
cli-cmd administration.profiles.activate</pre>

