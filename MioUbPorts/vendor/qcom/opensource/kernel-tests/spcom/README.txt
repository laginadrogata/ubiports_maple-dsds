
===============================================================================
spcom kernel API testing Documentation
===============================================================================

Usage: spcom_ktest.sh [OPTIONS]...

OPTIONS can be (defaults in parenthesis):
  -n, --nominal        nominal test cases
  -a, --adversarial    adversarial test cases
  -r  --repeatability  Run -a test 10 times
  -s  --stress         Run -a test 1000 times

Description:
The Secure Processor Communication (SPCOM) driver
provides communication to Secure Processor (SP) over G-Link transport layer.
It provides interface to both User Space Apps and kernel drivers.
User Space App shall use spcomlib for communication with SP.
kernel spcom api: include/soc/qcom/spcom.h

No message routing is used, but using the G-Link "multiplexing" feature
allows using a dedicated logical channel for HLOS and SP Application pair.
Each HLOS/SP Application can be either Client or Server or both,
Messaging is always point-to-point between two HLOS<=>SP applications.

The Secure Processor Subsystem (SPSS) hardware is supported on new chipsets.

The SPSS is loaded by HLOS PIL.
Once SPSS is up, it should "LINK UP" with the HLOS.
The SPSS main process (SKP) should connect to the HLOS via the "sp_kernel"
logical channel.
New SP Applications are loaded from HLOS via SPCOM by the SKP on the SP side.
Loading SP Application is not supported from the spcom kernel API,
but only spcomlib on user space.

Communication protocol is based on Request and Response.
Sending any Request data with invalid arguments to SKP will return a Response
data with error code.

Target support: msm8998

