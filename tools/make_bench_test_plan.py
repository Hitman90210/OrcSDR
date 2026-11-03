"""Generate the OrcSDR consolidation bench test plan PDF."""
import sys
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (KeepTogether, PageBreak, Paragraph, SimpleDocTemplate, Spacer,
                                Table, TableStyle)

OUT = sys.argv[1]
ss = getSampleStyleSheet()
H1 = ParagraphStyle("H1", parent=ss["Heading1"], fontSize=17, spaceAfter=6, textColor=colors.HexColor("#0b3d5c"))
H2 = ParagraphStyle("H2", parent=ss["Heading2"], fontSize=12.5, spaceBefore=10, spaceAfter=4,
                    textColor=colors.HexColor("#0b3d5c"))
BODY = ParagraphStyle("B", parent=ss["BodyText"], fontSize=9.2, leading=12)
SMALL = ParagraphStyle("S", parent=BODY, fontSize=8.3, leading=10.4)
CELL = ParagraphStyle("C", parent=BODY, fontSize=8.2, leading=10.2, alignment=TA_LEFT)
CODE = "<font face='Courier' size='8'>%s</font>"
NOTE = ParagraphStyle("N", parent=BODY, fontSize=8.8, leading=11.5, backColor=colors.HexColor("#fff4d6"),
                      borderColor=colors.HexColor("#e0b84a"), borderWidth=0.6, borderPadding=5,
                      spaceBefore=4, spaceAfter=8)


def c(text):
    return CODE % text


def section(title, intro, rows):
    """rows: (test, how, expected) -> checklist table with PASS/FAIL boxes and notes."""
    data = [[Paragraph("<b>#</b>", CELL), Paragraph("<b>Test</b>", CELL), Paragraph("<b>How</b>", CELL),
             Paragraph("<b>Expected</b>", CELL), Paragraph("<b>P / F</b>", CELL),
             Paragraph("<b>Notes</b>", CELL)]]
    for i, (test, how, expected) in enumerate(rows, 1):
        data.append([Paragraph(str(i), CELL), Paragraph(test, CELL), Paragraph(how, CELL),
                     Paragraph(expected, CELL), Paragraph("[&nbsp;&nbsp;] / [&nbsp;&nbsp;]", CELL), ""])
    t = Table(data, colWidths=[0.25 * inch, 1.2 * inch, 2.05 * inch, 1.95 * inch, 0.55 * inch, 1.0 * inch],
              repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#dbe9f3")),
        ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#9fb3c4")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f6f9fb")]),
        ("LEFTPADDING", (0, 0), (-1, -1), 3), ("RIGHTPADDING", (0, 0), (-1, -1), 3),
        ("TOPPADDING", (0, 0), (-1, -1), 2.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 2.5),
    ]))
    head = [Paragraph(title, H2)]
    if intro:
        head.append(Paragraph(intro, BODY))
        head.append(Spacer(1, 4))
    return [KeepTogether(head + [t])] if len(rows) <= 6 else head + [t]


def footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 7.5)
    canvas.setFillColor(colors.HexColor("#5b6b78"))
    canvas.drawString(0.6 * inch, 0.45 * inch,
                      "OrcSDR Tab5 - bench test plan for the 2026-11-03 consolidation (main 5c7e5c1)")
    canvas.drawRightString(7.9 * inch, 0.45 * inch, "Page %d" % doc.page)
    canvas.restoreState()


story = []
story.append(Paragraph("OrcSDR Tab5 - Bench Test Plan", H1))
story.append(Paragraph(
    "Covers everything changed on <b>2026-11-03</b>: the fork re-based onto upstream <b>cdc8181</b> "
    "(v0.5.0-beta.1 plus 95 commits, driver 0.9.4) with this fork's work re-applied. Branch "
    "<b>main</b> / <b>main-hoi278</b> at commit <b>5c7e5c1</b>. <b>Nothing in it has been compiled or "
    "run on hardware yet</b>, so expect the first build to need fixes.", BODY))
story.append(Spacer(1, 6))
story.append(Paragraph(
    "<b>Before flashing:</b> back up the firmware currently on the Tab5 (the flash skill's backup step, or "
    + c("esptool read_flash") + "), so you can return to it. If the new build misbehaves badly, the last "
    "pre-consolidation line is commit <b>a7e2db6</b> - still in history, buildable with "
    + c("git checkout a7e2db6") + ". Keep a serial monitor open for every test and save the log; most "
    "results below are serial lines. Pair the serial session first (" + c("tools/tab5_serial_auth.ps1") +
    ") so commands marked <i>(auth)</i> are accepted.", NOTE))

story += section("1. Build and size", "The one gate that decides whether this branch can ship.", [
    ("Clean build", "Build with your usual script on the Windows PC.",
     "0 errors. Note the warning count; new ones in " + c("channel_scan_plan.cpp") + ", "
     + c("main.cpp") + " scanner code or " + c("home_dashboard.cpp") + " are mine to fix."),
    ("App partition headroom", "Read the size report at the end of the build.",
     "Record % free. The last measured build had <b>33%</b>; upstream added 17 always-built files. "
     "Under ~10% is a problem."),
    ("Host tests", c("bash tools/test-band-profile.sh") + " (WSL or Git Bash).",
     c("channel_scan_plan PASS") + " plus band_profile and gmrs_channels PASS."),
])

story += section("2. Boot", "Watch the serial log from reset to the Home screen.", [
    ("Boot reaches Home", "Power-cycle with the RTL-SDR attached.",
     "Splash, then Home with a live spectrum. " + c("BOOT_SPLASH_SUMMARY") + " shows sd/wifi/rtl."),
    ("Self-checks honest", "Scan the log for " + c("_SELF_CHECK") + " lines.",
     "Each check prints <b>exactly one</b> line, _OK or _FAIL - never both. Note every _FAIL."),
    ("New self-checks", "Look for the two new ones.",
     c("ORC_SAME_SELF_CHECK_OK") + " and " + c("ORC_CHANNEL_SCAN_SELF_CHECK_OK") + "."),
    ("A failing check no longer stops boot", "Only if one of the shortwave/CB/airband/receiver-controls "
     "checks reports _FAIL.", "Boot still finishes: Wi-Fi, SD and radio all come up anyway."),
    ("Recent dashboards", "Open the dashboard list after upgrading.",
     "A GMRS entry may show as AM and AM as FT8 once (known, cosmetic). Open a few; it settles."),
])

story += section("3. Wi-Fi", None, [
    ("Connect to your router", "Settings - Connectivity - connect to your saved network.",
     "Connects (about 1 s slower than before: it scans all channels and takes the strongest AP)."),
    ("WPA-only router (if you have one)", "Join an older WPA (not WPA2) network.",
     "Joins. Before, ESP-IDF's WPA2-only default would refuse it."),
    ("Short password refused", "ADD HIDDEN / connect, type a 6-character password, CONNECT.",
     "Keyboard stays open titled " + c("PASSWORD: 8+ CHARACTERS") + ". 5 characters is allowed (WEP)."),
    ("Drop reason shown", "With Wi-Fi connected, switch the router off or walk out of range.",
     "Settings shows <b>Lost: &lt;reason&gt; - reconnecting</b>; serial " + c("RTL_WIFI_DISCONNECTED reason=N")
     + ". Reconnects when the AP is back."),
    ("Wrong password reason", "Connect with a wrong password.",
     "<b>Failed: wrong password</b> (or handshake timeout), not a bare 'Connection failed'."),
    ("RESET LINK button", "Settings - Connectivity - RESET LINK (beside CONNECT ON BOOT).",
     "Shows RESETTING..., serial " + c("RTL_WIFI_RESET_LINK queued") + " then "
     + c("RTL_WIFI_LINK_RECOVERY result=ok") + "; reconnects on its own."),
    ("RESET LINK over serial", c("RTL_WIFI_RESET_LINK") + " (auth).", "Same as above."),
    ("Antenna setting persists", "Set WI-FI ANTENNA to EXTERNAL (MMCX), reboot.",
     "Still EXTERNAL after reboot. <b>Previously it always reverted to INTERNAL.</b>"),
    ("Long SSID layout", "Save or scan a network with a long name.",
     "Status line, saved rows and scan buttons do not overlap their neighbours or the CONNECT button."),
])

story += section("4. Companion (LAN web console)", "Settings - Accessories &amp; Companion. Use a browser "
                 "and a PC on the same LAN.", [
    ("Default is view-only", "Enable the web console. Check the REMOTE CONTROL row.",
     "Reads <b>VIEW ONLY</b>. The page at http://&lt;tab5-ip&gt;/ shows status, spectrum, audio."),
    ("Control refused when off", "From the PC: " + c("curl -X POST --data volume_up http://&lt;ip&gt;/api/action"),
     c("403") + " with body " + c("control disabled") + "."),
    ("Control works when on", "Tap ALLOW (or " + c("RTL_WEB CONTROL ON") + "), repeat the curl.",
     c("202") + " / ok, and the volume changes on the Tab5."),
    ("Status reports it", c("RTL_WEB_STATUS"), "Ends with " + c("control=0") + " or " + c("control=1") + "."),
    ("Status JSON valid", "Open http://&lt;ip&gt;/api/status in the browser for a minute on several bands.",
     "Always parses (no " + c("nan") + "); page never blanks."),
])

story += section("5. FM broadcast", None, [
    ("SEEK up", "FM dashboard, Listen tab. Tap SEEK + &gt;&gt; from a station.",
     "Both SEEK buttons read STOP SEEK while it sweeps; stops on the next real station <b>above</b>. "
     "Serial " + c("RTL_FM_SEEK found frequency_hz=..") + "."),
    ("SEEK down", "Tap &lt;&lt; SEEK -.", "Stops on the next real station below."),
    ("Wraps at the edges", "Seek up from 107.9; seek down from 88.1.", "Wraps round and finds a real station."),
    ("Stop mid-sweep", "Tap either SEEK button while it is sweeping.",
     "Stops where it is; labels return to SEEK."),
    ("Preset scan", "Run the preset scan.",
     "Finds your real local stations only - not every channel (the old -70 dBFS test passed noise)."),
    ("Long RadioText", "Station with long RDS RadioText, Listen tab.",
     "Text stays inside its area; does not spill into STEREO VU."),
    ("RTL_PRESET_TUNE", c("RTL_PRESET_TUNE 1") + " then " + c("RTL_PRESET_TUNE 12") + " (auth, needs 12 presets).",
     c("RTL_PRESET_TUNE_OK index=1") + " / " + c("index=12") + ". Before, it never matched."),
])


story += section("6. GMRS / marine channel scanner (new - calibrate first)",
    "Thresholds are <b>placeholders</b> (min SNR 20 dB, squelch -75 dBFS) from the old DSP chain. "
    "Calibrate them before judging the scanner. You need something that transmits on GMRS/FRS - an FRS "
    "handheld on low power a few metres away is ideal (receive-only on the Tab5; transmitting on GMRS "
    "channels needs your own licence).", [
    ("SCAN button appears", "Home: tune 462.5625 MHz (FRS 1).",
     "The STEP SIZE box becomes <b>CH 1 / SCAN</b>. On other bands it is the normal stepper."),
    ("Noise reading", "On an idle channel: " + c("RTL_CHANNEL_PROBE") + " five times.",
     "Note snr_db (noise). Expect roughly 5-15 dB, busy=0."),
    ("Signal reading", "Key the handheld on that channel; probe again.",
     "Note snr_db (signal), offset_hz near 0, busy=1 if above the threshold."),
    ("Set the threshold", c("RTL_CHANNEL_MIN_SNR &lt;dB&gt;") + " (auth) halfway between the two readings.",
     c("RTL_CHANNEL_MIN_SNR_OK") + ". Survives a reboot (check " + c("RTL_CHANNEL_MIN_SNR") + ")."),
    ("Scan and stop", "Tap SCAN, then key the handheld on another channel.",
     "Shows SCANNING - STOP, the dial walks channels, then <b>HOLD</b> on the busy channel; "
     + c("RTL_CHANNEL_SCAN hold") + "."),
    ("Resume", "Stop transmitting.", "About 2.5 s later it resumes: " + c("resume ... reason=quiet") + "."),
    ("Stop", "Tap the SCAN box while scanning or holding.", "Stops; box reads SCAN."),
    ("Tune by hand stops it", "While scanning, tap the spectrum or step the dial.", "Scan stops."),
    ("Lockout", c("RTL_CHANNEL_LOCK R15") + ", then " + c("RTL_CHANNEL_LOCK_LIST") + "; reboot.",
     "R15 listed and skipped by scans; still locked after reboot. " + c("RTL_CHANNEL_UNLOCK R15") + " undoes."),
    ("Old lockouts kept", "If you had GMRS lockouts on the old firmware: " + c("RTL_CHANNEL_LOCK_LIST") + ".",
     "The same channels are still locked."),
    ("Dwell is long enough", "After a few minutes of scanning: " + c("RTL_CHANNEL_SCAN_STATUS") + ".",
     c("stale_skips") + " stays near 0. If it climbs, the 400 ms dwell is too short - tell me."),
    ("Marine", "Tune 156.800 (ch 16) and repeat SCAN/probe if you have marine traffic or a handheld.",
     "CH 16 / SCAN appears; stops on traffic."),
])

story += section("7. NOAA weather and SAME alerts (new)", None, [
    ("Decoder self-test", c("RTL_SAME_SELFTEST"), c("RTL_SAME_SELFTEST ok failed_step=none") + "."),
    ("Weekly test alert", "Listen on your local NOAA channel during the Required Weekly Test "
     "(usually Wednesday late morning; check your NWS office).",
     "Serial " + c("RTL_SAME alert org=WXR event=RWT ...") + "; " + c("RTL_SAME_LAST") + " lists your county FIPS."),
    ("Status", c("RTL_SAME_STATUS") + " while on the weather band.", "listening=1; counters move after a burst."),
])

story += section("8. Radio commands and other bands", None, [
    ("LoRa modem", c("RTL_LORA_MODEM") + ", then " + c("RTL_LORA_MODEM 11 250000") + " (auth).",
     "Status line, then " + c("RTL_LORA_MODEM_OK sf=11 bandwidth_hz=250000") + "."),
    ("LoRa self-test", c("RTL_LORA_SELFTEST"), c("RTL_LORA_SELFTEST ok") + "."),
    ("CB squelch", c("RTL_SQUELCH -60") + " (auth), reboot, " + c("RTL_SQUELCH") + ".",
     c("RTL_SQUELCH_STATUS dbfs=-60") + "; CB dashboard squelch matches."),
    ("ADS-B positions", "ADS-B with your receiver location set, 15+ minutes.",
     "No aircraft jumps across the radar. At TRACE, " + c("RTL_ADSB_POSITION_REJECTED") + " is rare."),
    ("Driver 0.9.4 on your V4", c("RTL_DRIVER STATUS") + "; then listen on FM, NOAA, airband.",
     "Reception as good as before. If anything is off-frequency or worse: " + c("RTL_DRIVER OFFSET OFF") +
     " and compare (offset tuning is new and on by default)."),
    ("Silent commands now answer", "Without pairing: " + c("RTL_LORA_AUTO ON") + ", " + c("RTL_WEB ON") + ".",
     c("RTL_LORA_AUTO_ERROR auth_required") + ", " + c("RTL_WEB_ERROR auth_required") + "."),
    ("Live screenshot", c("python tools/screenshot_tab5.py") + " (snapshot mode) on a live screen.",
     c("UI_SNAPSHOT_DONE") + " and a BMP on the PC."),
])

story += section("9. SD card and data packs", None, [
    ("Free space is real", "Settings - Storage.", "SD free is <b>less than</b> the card size (it used to equal it)."),
    ("Install a pack", "Data &amp; Maps - CHECK FOR UPDATES - install FAA AVIATION.", "Installs; message names any failure."),
    ("Remove frees space", "Remove that pack; " + c("SD_LIST /orcsdr/data") + ".",
     "No " + c(".bak") + " or " + c(".ver") + " left behind for it."),
    ("Copy scripts on PowerShell 5.1", c("powershell -File tools\\copy_to_tab5_sd.ps1 ...") + " (Windows PowerShell, not pwsh).",
     "Copies (pairs first). Before, these failed on 5.1 and never authenticated."),
    ("Maps", "Check the map list.", "Older " + c("lane_county_map.idx") + " shows UNSUPPORTED FORMAT; FILE PRESERVED (expected)."),
])

story.append(Spacer(1, 8))
story.append(Paragraph("When you are done", H2))
story.append(Paragraph(
    "Send me the serial log and this sheet's FAIL rows, plus the partition headroom and the two scanner "
    "SNR readings. Once the build passes, remove the <b>Untested build</b> warning at the top of "
    + c("README.md") + ".", BODY))

doc = SimpleDocTemplate(OUT, pagesize=letter, leftMargin=0.6 * inch, rightMargin=0.6 * inch,
                        topMargin=0.6 * inch, bottomMargin=0.7 * inch,
                        title="OrcSDR Tab5 Bench Test Plan", author="OrcSDR fork")
doc.build(story, onFirstPage=footer, onLaterPages=footer)
print("wrote", OUT)
