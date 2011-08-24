wview2wurapid

Copyright (c) 2011 John Goerzen

Portions Copyright (c) wview developers

License in COPYING.

        This source code is released for free distribution under the terms 
        of the GNU General Public License.

------------------------------------------------------------
Introduction
------------------------------------------------------------

This program permits you to send rapid-fire updates to
Weather Underground (www.wunderground.com) from wview
(www.wviewweather.com).

------------------------------------------------------------
Prerequisites
------------------------------------------------------------

You will need:

 * bash
 * logger (standard on most systems)
 * wget
 * An unpacked wview source tree

------------------------------------------------------------
wview configuration
------------------------------------------------------------

First, open the management interface, and:

 * On the Station tab, set:
    Sensor polling interval to 15 seconds
    Data push interval to 15 seconds

 * On the Services tab, enable Alarms (wvalarmd).

   You don't need to define alarms, but wvalarmd provides
   the data feed to outside programs.

Save the settings and restart wview.

NOTE: If you set the interval longer than 15 seconds, you will have to
make some changes several places in the source.

------------------------------------------------------------
Configuring wview2wurapid
------------------------------------------------------------

First, edit Makefile.  Change the very top line to point to your
unpacked wview source tree.  Don't add "/common" at the end, but there
should be a /common directory inside the WVIEWSOURCEPATH.

Next, edit wview2wurapid.sh.  You will need to edit three lines -- the
ID, PASSWORD, and DATAFEED.

------------------------------------------------------------
Running wview2wurapid.sh
------------------------------------------------------------

You may wish to define this on a wview init script.  Or, you could
define a cron job to make sure that things are running.
wview2wurapid-watchdog.sh is one example; edit WVIEW2WURAPID in it and
then deploy as:

*/10 * * * * /path/to/wview2wurapid-watchdog.sh
