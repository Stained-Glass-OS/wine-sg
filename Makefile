# wine-sg -- Wine built for Stained Glass OS.
#
#   make build     build Wine (about 12 minutes on 12 cores)
#   make install   install to $(PREFIX), default /opt/wine-sg
#   make test      the gate: prove 32-bit Windows code runs with no i386 Linux
#   make deb       build the .deb
#
# The reason this repo exists is one configure flag: --enable-archs=i386,x86_64.
# See ADR 0002 and ADR 0005 in the stained-glass repo.

PREFIX  ?= /opt/wine-sg
DESTDIR ?=
JOBS    ?= $(shell nproc)

.PHONY: all build install test deb deps clean distclean lint test-rasdial test-taskdialog test-msitransform test-shellconsole test-pssig test-robocopy test-jscript test-darkmode test-taskbar test-explorer-dark test-d2dlayer test-d2dsvg test-kbchar test-d2dgraph test-advcolor test-focusactivate test-drvkey test-mmdevkey test-ofn test-selflink test-unixexec test-taskview-linux test-xproc-vk test-firstpaint test-fileopadmin test-msiallusers test-verblabel test-taskbarlooks test-fileopresponsive test-mapdrive test-openwith test-drag test-deskhover test-deskerase test-dpifollow test-hungwake test-savename test-folderspeed test-shortcut test-topbar test-msiseltree test-statuscount test-folderwatch test-dcompalpha test-spooler test-desktopbg test-layeredblend test-linuxicon test-shellnew test-glassvk test-elevfolders test-displaycfg test-pss test-pyimports test-ownerrights test-ptracecap test-proxyblanket test-comlauncher test-cloakpaint test-gpupriority test-dbghelpinline test-regfsync test-sysversions test-drivewatch test-systemtemp test-desktopfront test-mcastif test-prtsc test-stiinf test-usbtree test-gpahook test-autoplay test-envreload test-iconhandler test-gdipfamily test-dirshare test-elevtray test-anotherwindow test-threadclass test-fetabs test-linuxbash test-embedmaxgrab test-toastactivate test-netfx3 test-smbshare test-smbview test-netuse test-mapcred test-wmiprinter test-cupsmodel test-devkeyread test-cupsrefresh test-pentouch test-drvpkg test-psdriver test-unidrvxl test-unidrvrast test-psplugin test-cpsui test-drvui test-netport test-printproc test-dwsubst test-ptcaps test-smoothshapes test-iconframes test-touch test-keybutton test-hiddenmax test-tlbbase test-comsvc test-privacysettings test-dwspacing test-msgboxicons test-credkeyring test-latepen test-wininet-listentimeout test-onlineid-ticket test-richtxsrv2 test-d2dsharedview test-rotransformerror test-cputime test-fileinfo test-procsettings test-memprotect test-userapis test-kbmisc test-cpusets test-werreg test-devfamily test-aclentries test-richinit test-bufpaint test-dwmattrs test-muipath test-stockicon test-enumjobs test-replacefile test-svcdepend test-sysshutdown test-advsec test-shcorebits test-fontquery test-guithread test-ipnotify test-apprestart test-minidumpx test-joblimits test-fmtsupport test-printnotify test-diskinfo test-perfcount test-taskdisp test-taskinfo test-taskxml test-bitsjob test-bitsmore test-mmcss test-devtree test-pinterfaceiid test-taskprogress test-winappid test-childpolicy test-smallstubs2 test-asynclock test-uilangs test-shellapp test-shelldisp test-wshshell test-textstreams test-fsoitems test-wshnet test-vbsbuiltins test-vbsdyncode test-jsstubs test-nsplookup test-wscatalog test-gdipquality test-hotkeyexec test-zipfolder test-thumbbar test-xplacement test-tsfsinks test-certresync test-inputpane test-jumplist test-evtquery test-audiotopo test-dxcore test-dmanip test-imglist2 test-lvtv test-toolbar2 test-ccmisc test-ole32misc test-oapict test-tlbcreate test-roerror test-guires test-powerreq test-ncryptksp test-cryptrest test-etwtrace test-uiaevents test-msgfilter test-uiabridge test-shellmisc test-oleaccstd test-sockmisc test-netlist test-bestroute test-fileflags test-dwritemisc test-certfind test-pfxexport test-smallstubs3 test-aptshutdown test-cmifaces test-cmnotify test-powerschemes test-capsdhcp test-procquota test-credprotect test-creduiwin test-wtsnotify test-appdefault test-propkeyname test-setupcm test-urlmonparse test-urlmonzone test-boundary test-devicemap test-gentable test-perflib test-ncryptwrap test-netuserinfo test-wintrusthelp test-wtssession test-accdisp test-accprop test-d3d11featx test-d3d11misc test-comctlstub test-comdlgitem test-msxmlstub test-shlwapistub test-blockreason test-fwenv test-regmulti test-shellhook test-bcryptconfig test-bcryptoaep test-bcryptresolve test-crypt32aux test-dnsapihelp test-iphlpapient test-powerlegacy test-tapiline test-dxgistat test-dxgiswap test-jpegexif test-wicxform test-dsound-stubs test-lockserver test-mf-misc test-mfplat-buffers test-mfplat-bytestream test-wgi-device test-wgi-stubs test-wgst-mpegaudio test-wgst-wmreader test-winmm-misc test-wmc-stubs test-wmdev-stubs test-wmedia-stubs test-wmspeech test-wmspeech2 test-xinput-audio test-mitigate test-usersessions test-adocmd test-dtdcode test-ebrowser test-folderview test-jsonweb test-mimebody test-mimeopt test-mimesave test-msxmlcoll test-msxmlpersist test-mxwdom test-ofnull test-printfldr test-propbag test-propvar2 test-propvec test-pvstring test-richedoc test-richeedit test-richepara test-richesearch test-richesel test-secidom test-setupclass test-setupnodeprop test-setupspace test-sfileinfo test-sfparse test-shexecargs test-shlinkstub test-tabmru test-tbsep test-webverbs test-createfile test-fcache test-fileioctl test-fsmisc test-globalmem test-inputapi test-iosb test-lznt1 test-monhandle test-ntdllmisc test-ntinfoval test-olecompat test-pipeid test-profile test-pseudowait test-registry test-rpcbind test-rtlquery test-sppi test-svcdup test-svcfail test-svcname test-uimisc test-validdata test-wca test-winplace test-writevm test-crypt32cs test-crypt32dec test-crypt32msg test-mscoree-hash test-mscoree-sn test-netapi32ground test-netwrite test-regstorethrough test-suspend test-tapihandles test-tapiphone test-tapitrans test-winhttpground test-wininetground test-wintrustadmin test-wintrustcat test-wintrustcatw test-wintrustfind test-ws2ground test-wsreader test-wswriter test-wtsaction test-d2dctl test-d2dmesh test-d2dtarget test-d3d113res test-d3dshare test-d3dxline test-dwbidi test-dwfaceref test-dwhit test-dxgidup test-gdipimg test-gdipstat test-metawriter test-uiacache test-uiacom test-uiacpat test-uiapat test-wicblock test-wicenc test-wicicc test-wicinfo test-wicquery test-wicsave test-dinput-misc test-dmcontent-styletracks test-dmime-audiopath test-dmime-perf test-dmime-tracks test-dmloader-streams test-dplayx-dp4 test-dplayx-lobby test-dplayx-sp test-dplayx-v1 test-dpnet-session test-evr-filter test-evr-mixer test-evr-presenter test-mfme test-mfplay test-mfrest-helpers test-mfsrcsnk test-qdvd-nav test-qedit-amstream test-qedit-mediadet test-qedit-msvfw32 test-qedit-timeline test-quartz-graph test-quartz-vmr test-sapi-mmaudio test-sapi-stream test-sapi-token test-sapi-voice test-wgdmo-colorconv test-wgdmo-resampler test-wgdmo-wma test-wgrest-sink test-wgrest-source test-wgrest-wmreader test-wgvid-aacdec test-wgvid-h264enc test-wgvid-vproc test-wgvid-wmvdec test-wmp-state test-wmv-config test-wmv-misc test-wmv-reader test-winstasd test-wtstoken test-logonuser test-runasuser test-accesscheck test-actctxfast test-combasemisc test-dirchange test-endtask test-enumsettings test-procinfo2 test-sdmisc test-sysclasses test-tfcreate test-d2dtxt test-d2dvb test-d3drmface test-d3drmframe test-d3drmpath test-d3drmtexdev test-d3dxanim test-d3dxconv test-d3dxddspal test-d3dxdxt test-d3dxfont test-d3dxisect test-d3dxmesh test-d3dxsavetex test-d3dxtga test-dcompobj test-dwfactref test-dwfont test-dwset test-dwsetaxes test-dwtab test-dwwrap test-gdipdisp test-gdipdrv test-gdipinit test-gdipmeas test-gdipnn test-gdiprgn test-oleaccproxy test-uiaiface test-wicdds test-wichbm test-wicpnginfo test-dinputtodo test-dmcontent-band test-dmcontent-compos test-dmcontent-style test-dmusic-ports test-dmusic-synth test-mfrest2 test-quartz-rest test-wmvw-header test-wmvw-profile test-wmvw-writer test-wmvw-xml test-xact-wave test-adoconn test-adorecords test-cwmo test-dinput-hidusage test-dinputleft test-dmcompose test-dmcontent-play test-dmime-conv test-dmscript test-evr-classfactory test-evr-mfexports test-mc-audiostate test-mf-sequencer test-mfplat-mediabuf test-mfplat-mtconv test-mfplat-pdser test-mfplat-plugin test-mfplat-vidrep test-mfplat-vih test-quartz-collections test-quartz-fullscreen test-quartz-misc test-wgasync test-winmmthru test-winmmtodo-dsound test-winmmtodo-winmm test-wmp-collections test-wmvw-sysprof test-adostream test-cc3 test-diskspace2 test-mimemsg test-mimeprops test-propconv test-propstore2 test-recentdocs test-richpaste test-setupprop test-shcore2 test-shdataobj test-shellord test-idealproc test-monikeredge test-regorder test-volumetime test-bcryptground test-crypt32ground test-dnsapiqueryex test-msicomppath test-msicost test-msifilever test-msiground test-msipatch test-msiprodinfo test-msisignature test-ncryptdpapi test-secur32sasl test-secur32schannel test-taskschdground test-wbemcom test-wbemproc test-wbemsettings test-winhttpsocket test-wininetcrack test-wininetnoreply test-wintrusthashonly test-wmic test-wsarray test-wsheap test-wslistener test-wsstart test-wsvalue test-d2dbmpcc test-d3drmclone test-d3drmfaces test-d3drmmb test-d3dxsampler test-dcomp3d test-dcompsurface test-dwritewoff test-gdipconv test-gdipdual test-uiacachetree test-uiagroup test-uianorm test-uiaopts test-wicico test-wicidxpal test-hklmsd test-amstream-alloc test-amstream-smoke test-avifil32-palchange test-avifil32-smoke test-concrt-context test-concrt-exc-locks test-concrt-helpers test-concrt-queue-iter test-crt-cgets test-crt-concurrency test-crt-context-yield test-crt-dbgheap test-crt-eh-helpers test-crt-exc-misc test-crt-find32i64 test-crt-heap test-crt-imagebase test-crt-internals test-crt-ismbb test-crt-iswcsym test-crt-mbcasemap-data test-crt-misc test-crt-norm test-crt-old-handlers test-crt-platform test-crt-portio test-crt-printf test-crt-scanf test-crt-string-s test-crt-systime test-crt-uchar test-crt-win32-wrappers test-devenum-smoke test-dmsynth-dls1 test-dpvoice-factory test-dsdmo-smoke test-dxva2-smoke test-evr-fullscreen test-gamebar-smoke test-mciavi32-smoke test-mciqtz32-smoke test-media-misc-smoke test-mediacontrol-smoke test-mf-seekpause test-mf-smoke test-mfplat-smoke test-mfreadwrite-smoke test-mmdevapi-smoke test-mp3dmod-dmo test-msacm32-metrics test-msacm32-smoke test-msdmo-smoke test-msvcp-access-forms test-msvcp-atomic-wait test-msvcp-c-collate test-msvcp-codecvt-utf test-msvcp-collate test-msvcp-eptr-swap test-msvcp-errcat test-msvcp-fs-misc test-msvcp-locale-ops test-msvcp-locinfo test-msvcp-makeloc test-msvcp-messages test-msvcp-misc-older test-msvcp-money test-msvcp-old-exports test-msvcp-old-forms test-msvcp-old-members test-msvcp-platform test-msvcp-random-mp test-msvcp-stream-move test-msvcp-streambuf test-msvcp-threads-c test-msvcp-timeget test-msvcp-ushort-read test-msvcp-ushort-read-older test-msvcp-wcomplex test-msvcp-wstod test-msvcp-xexc test-msvcp-xfsopen test-msvcp-yarn-facetbase test-msvcp100-misc test-msvcp140-2-smf test-msvcp140-crt-wrappers test-msvcp60-assign test-msvcp60-complex test-msvcp60-dateorder test-msvcp60-doraise test-msvcp60-exceptions test-msvcp60-getcat test-msvcp60-locale-call test-msvcp60-std-exceptions test-msvcp60-streambuf-misc test-msvcp60-timeget test-msvcp60-traits-assign test-msvcp70-complex test-msvcr100-task-canceled test-msvfw32-smoke test-qasf-smoke test-qcap-smoke test-qdvd-smoke test-quartz-dsound-focus test-quartz-graph-forward test-quartz-vmr9-alloc test-rtworkq-platform-events test-ucrt-dmath test-vcomp-dynamic-i8 test-vcrt-module-wrappers test-wmp-moniker test-wmvcore-playlist-burn test-wmvcore-smoke test-advapi32-efs test-advapi32-hwprofile test-advapi32-reflection test-advapi32-servicebits test-bcrypt-aeswrap test-bcrypt-algnotfound test-bcrypt-decryptflags test-bcrypt-importpair test-bcrypt-propnotsupported test-combase-ctxcallback test-combase-ctxprops test-combase-defctx test-combase-handlerconnected test-combase-hpalette test-combase-logicalid test-combase-stdmarshal-disconnect test-combase-stdmarshal-null test-crypt32-adddisposition test-crypt32-closecheck test-crypt32-hexa test-crypt32-universalstring test-cryptobuffer-convert test-cryptowinrt-inspectable test-gdi32-smalltextout test-kernel32-pipesession test-kernel32-pipestate test-kernel32-regquota test-kernel32-symlinka test-kernel32-ums test-kernel32-wow64selector test-kernel32-zombifyactctx test-kernelbase-affinityupdate test-kernelbase-consolemouse test-kernelbase-georfc1766 test-kernelbase-shregcreate test-kernelbase-shregdel test-kernelbase-urlescapecomponent test-kernelbase-urlfixup test-kernelbase-urlisappliable test-ntdll-bitmap-backward test-ntdll-compactheap test-ntdll-compressioninfo test-ntdll-filealign test-ntdll-flushwrites test-ntdll-fsobjectid test-ntdll-iopending test-ntdll-placeholder test-ntdll-powercaps test-ntdll-querydir-async test-ntdll-regalign64 test-ntdll-regioninfo test-ntdll-subsystem test-ntdll-symlink test-ntdll-timerqueue-event test-ntdll-xpress-edges test-ntdll-xpress test-ntoskrnl-callback test-ntoskrnl-confinfo test-ntoskrnl-csq test-ntoskrnl-dpc test-ntoskrnl-dpcqueue test-ntoskrnl-drvext test-ntoskrnl-errorlog test-ntoskrnl-harderror test-ntoskrnl-iocreate test-ntoskrnl-iotimer test-ntoskrnl-loadimage test-ntoskrnl-mdl test-ntoskrnl-netiotest test-ntoskrnl-ntddi test-ntoskrnl-obref test-ntoskrnl-pnptest test-ntoskrnl-procgroup test-ntoskrnl-procname test-ntoskrnl-queue test-ntoskrnl-regs test-ntoskrnl-reinit test-ntoskrnl-resclaim test-ntoskrnl-shutdown test-ntoskrnl-startio test-ole32-cachefreeze test-ole32-callas test-ole32-colorset test-ole32-enumskip test-ole32-handler test-ole32-monikers test-ole32-olestreamex test-ole32-proptimes test-ole32-setdata test-ole32-staticdata test-ole32-stgconvert test-ole32-stgopenex test-ole32-viewcallas test-oleaut32-hfont test-oleaut32-propbag-qa test-oleaut32-propframe-initial test-oleaut32-recinfo test-oleaut32-tlcallas test-oleaut32-typelibname test-onlineid-inspectable test-rpcrt4-async test-rpcrt4-async-rpc test-rpcrt4-canceltimeout test-rpcrt4-clearout test-rpcrt4-comtimeout test-rpcrt4-convert test-rpcrt4-cstruct test-rpcrt4-fcwstring test-rpcrt4-httprts test-rpcrt4-ifids test-rpcrt4-pipe test-rpcrt4-ptrgap test-rpcrt4-unregifex test-user32-ddedisconnect test-webcore-inspectable test-webcore-mapview test-win32u-coloradjust test-win32u-getpixelclip test-win32u-lockwindowupdate test-win32u-mouseinpointer test-win32u-openadapterhdc test-win32u-openadaptername test-gdi32-aspect test-gdi32-colorspace test-gdi32-emf-escapes test-gdi32-engclip test-gdi32-gcpclass test-gdi32-opentype test-gdi32-updateicmregkey test-ntdll-sppi test-user32-animrects test-user32-dataobj test-user32-ddelist test-user32-ddepartner test-user32-devnotify-ansi test-user32-dpihosting test-user32-msgbox-help test-user32-setdisplayconfig test-user32-touchht test-user32-winnls-ime test-usp10-justify test-win32u-ddescape test-win32u-eto-numerics test-win32u-flash-timer test-win32u-hkl-alias test-win32u-memfont test-win32u-menu-autodismiss test-win32u-qai test-win32u-spi-access test-win32u-wgl-sharelists test-richrc test-widl-pipe test-fastactiveds test-fastaddmon test-fastado test-fastado2 test-fastadsldp test-fastadvpack test-fastagile test-fastappdata test-fastappmodel test-fastatl-ax test-fastatl-ax2 test-fastatl-misc test-fastatl30 test-fastbrowseui test-fastcab test-fastcdlg test-fastcloudasn1 test-fastcloudkbd test-fastcloudmapi test-fastcloudnewdev test-fastcloudpcap test-fastcloudprntvpt test-fastcloudqos test-fastcloudspool test-fastcloudsppc test-fastcloudtbs test-fastcloudurl test-fastcloudwsh test-fastclusapi test-fastcomsvcs test-fastcoremsg test-fastdataexchange test-fastdbgeng test-fastddraw test-fastdevenum test-fastdhtmled-b2 test-fastdhtmled test-fastdsui test-fastdwjust test-fastdx8vb test-fastdxgirc test-fastesent test-fastesent2 test-fastextrac32 test-fastfileio test-fastfusion test-fastgameux test-fastglobalization test-fasthhctrl test-fasthidtranslate test-fasthnetcfg test-fasthnetcfg2 test-fasthttpapi test-fastie-bs test-fastie test-fastie-mshtml test-fastie-riched test-fastie-urlmon test-fastieframe-b1 test-fastieframe-events test-fastieframe test-fastieframe-host test-fastieframe-move test-fastieframe-newwin test-fastieframe-showui test-fastieshortcut test-fastimm-ime test-fastinetcomm test-fastinetedit test-fastinetintl test-fastinseng test-fastinspect test-fastiphlp test-fastiq test-fastitss test-fastjsb1 test-fastjscall test-fastjserr test-fastjsintl test-fastjsmisc test-fastjsmode test-fastjsproxy test-fastjstyped test-fastlongtail test-fastmapi test-fastmapi2 test-fastmlang test-fastmpr test-fastmprapi test-fastmsctf test-fastmsctfb1 test-fastmsctfb2 test-fastmsctfprof test-fastmsdaps test-fastmsdapsrow test-fastmsdasql test-fastmshtml test-fastmshtmladopt test-fastmshtmlb1 test-fastmshtmlb2 test-fastmshtmlb3 test-fastmshtmlb4 test-fastmshtmlb5 test-fastmshtmlcssom test-fastmshtmldm test-fastmshtmldoc7 test-fastmshtmldom test-fastmshtmldomex test-fastmshtmlerr test-fastmshtmlevent test-fastmshtmlfile test-fastmshtmlmarkup test-fastmshtmlmedia test-fastmshtmlmisc test-fastmshtmlmo test-fastmshtmlopen test-fastmshtmlpointer test-fastmshtmlrangecmd test-fastmshtmlrc test-fastmshtmlsel test-fastmshtmltrav test-fastmshtmlundo test-fastmshtmlunits test-fastmsi test-fastmsiaut test-fastmsib1 test-fastmsib2 test-fastmsiini test-fastmsimode test-fastmsimtf test-fastmsipkg test-fastmsiprov test-fastmsisrc test-fastmsiusage test-fastmsscript test-fastmsxml-b1 test-fastmsxml-b2 test-fastmsxml test-fastmsxml3 test-fastmsxmlmisc test-fastmsxmlsax test-fastmsxmlwriter test-fastmsxmlxslt test-fastnet-emit1 test-fastnet-host1 test-fastnet-meta1 test-fastnet-sn1 test-fastnet-sn2 test-fastnetacct test-fastnetapi test-fastnetbios test-fastnetmisc test-fastninput test-fastodbccp32 test-fastoldgfx test-fastole32rc test-fastoledb32 test-fastoledlg test-fastopencl test-fastpackager test-fastpdh test-fastpf test-fastprintui test-fastrasapi test-fastrasbook test-fastregini test-fastrometa2 test-fastrometadata test-fastrstrtmgr test-fastscrobj test-fastsensapi test-fastservices test-fastsetjob test-fastsetupapi test-fastsetupimg test-fastshdocvw test-fastshdocvw2 test-fastshell32rc test-fastshlwapi3 test-fastshmisc test-fastsmbios test-fastsockiosb test-fastsubst test-fastsvcenum test-fasttapi test-fastthreadpool test-fastuiarc test-fastuiarepeat test-fasturlmon-b1 test-fasturlmon-b2 test-fasturlmon test-fastvbb1 test-fastvbenum test-fastvberr test-fastvbfor test-fastvbparens test-fastvbs test-fastvidcap test-fastvssapi test-fastwbemdisp test-fastwbemsink test-fastwebabort test-fastwebb1 test-fastwebdesc test-fastwebhdr test-fastwebhttp test-fastwebserv test-fastwebservices test-fastwebsvc2 test-fastwebtypes test-fastwinhlp test-fastwinhttp test-fastwininet-b1 test-fastwininet test-fastwininetenum test-fastwininetgroup test-fastwininetopt test-fastwininetrc test-fastwinstorage test-fastwintypes test-fastwinui test-fastwinui-sched test-fastwlanrc test-fastwpc test-fastwstorage2 test-fastwuapi test-fastwuapi2 test-fastwuapiset test-fastxcfg test-fastxmllite test-fastxolehlp test-g-shared-d3d11 test-g-shared-fence test-g-shared-kmt test-g-shared-kmtmutex test-g-shared-kmtsync test-msix2 test-fastb-abterm test-fastb-acct test-fastb-activeds test-fastb-addmon test-fastb-adesk test-fastb-advpack test-fastb-amsi test-fastb-apphelp test-fastb-authz test-fastb-bitsup test-fastb-bt test-fastb-btdev test-fastb-bthprops test-fastb-cdm test-fastb-cfgmgr test-fastb-cmeject test-fastb-cmenu test-fastb-cmenuids test-fastb-comcaller test-fastb-comctl test-fastb-compose test-fastb-conalias test-fastb-cordebug test-fastb-corefix test-fastb-creddom test-fastb-cryptui test-fastb-d3d11vp test-fastb-d3dpatch test-fastb-d3drm test-fastb-d3drm2 test-fastb-d3drm3 test-fastb-d3dx test-fastb-d3dx10 test-fastb-d3dx11 test-fastb-d3dxfx test-fastb-d3dxload test-fastb-d3dxtex test-fastb-dad test-fastb-dbgeng test-fastb-dbghelp test-fastb-ddewild test-fastb-dia test-fastb-dns test-fastb-dwmthumb test-fastb-dwrite test-fastb-dxgi test-fastb-ebstate test-fastb-edp test-fastb-efill test-fastb-enummui test-fastb-fipsmode test-fastb-fltlib test-fastb-fontkey test-fastb-fvdual test-fastb-gapbluetooth test-fastb-gapcf test-fastb-gapconnectivity test-fastb-gapd2d test-fastb-gapd3dx test-fastb-gapd3dx2 test-fastb-gapdbg test-fastb-gapdbgeng test-fastb-gapdcomp test-fastb-gapddraw test-fastb-gapdrm test-fastb-gapexp test-fastb-gapfence test-fastb-gapgdip test-fastb-gapgp test-fastb-gaphlink test-fastb-gaphostname test-fastb-gapimagehlp test-fastb-gapimagehlp2 test-fastb-gapinseng test-fastb-gapmisc test-fastb-gapmscms test-fastb-gapmscoree2 test-fastb-gapmsscript test-fastb-gapmstask test-fastb-gapogl test-fastb-gapoledb test-fastb-gappropsys test-fastb-gapprov test-fastb-gapre test-fastb-gapsechost test-fastb-gapshcore test-fastb-gapshell test-fastb-gapspool test-fastb-gapsti test-fastb-gapuia test-fastb-gapwbemprox2 test-fastb-gapwd3d test-fastb-gapwd3d9 test-fastb-gapwia test-fastb-gapwic test-fastb-gapwintab test-fastb-gapwsdapi test-fastb-gapwsock32 test-fastb-gdip test-fastb-gdipsmall test-fastb-geo test-fastb-gldef test-fastb-hidstack test-fastb-hlink test-fastb-imm test-fastb-ink test-fastb-itss test-fastb-kbdload test-fastb-kernel test-fastb-kfm test-fastb-kvol test-fastb-logonsess test-fastb-mapdebug test-fastb-mpr test-fastb-mscms test-fastb-mscoree test-fastb-msgdecstream test-fastb-msgstream test-fastb-msident test-fastb-mssign test-fastb-mstask test-fastb-ncrypt test-fastb-netapi test-fastb-netcfg test-fastb-netconn test-fastb-ntds test-fastb-odbc test-fastb-ole1 test-fastb-oledb32 test-fastb-opc test-fastb-physstore test-fastb-propsys test-fastb-rbin test-fastb-rdrop test-fastb-retxtsrv test-fastb-ribbon test-fastb-rpcifids test-fastb-rtmmcss test-fastb-rtwq test-fastb-rtwqjoin test-fastb-rtxs test-fastb-sarrelease test-fastb-sdimach test-fastb-secacl test-fastb-sechost test-fastb-seckb test-fastb-sens test-fastb-setupunmapped test-fastb-sfview test-fastb-sfvmsg test-fastb-shcore test-fastb-shell2 test-fastb-shell3 test-fastb-shell4 test-fastb-shfileop test-fastb-shfileop2 test-fastb-shimg test-fastb-shitem test-fastb-shmisc test-fastb-sxs test-fastb-syscur test-fastb-sysproto test-fastb-taskschd test-fastb-tdh test-fastb-tdhprop test-fastb-thprio test-fastb-tom test-fastb-tom2 test-fastb-tools test-fastb-ucv test-fastb-ucvagile test-fastb-uia test-fastb-uiaannot test-fastb-uiacache test-fastb-uiafind test-fastb-uianc test-fastb-uiatext test-fastb-uncnp test-fastb-userenv test-fastb-uxanim test-fastb-vfold test-fastb-wbem test-fastb-wbemps test-fastb-wer test-fastb-wgicustom test-fastb-wic test-fastb-winrtnet test-fastb-winrtstub test-fastb-winsta test-fastb-wintab test-fastb-wlan test-fastb-wmiu test-fastb-ws2 test-fastb-wscript test-fastb-wsd test-fastb-wsock test-fastb-wspool test-fastb-wts test-fastb-xmllite test-fastb-xof test-fastb-xofsave test-fastb-zipmenu test-richepara2 test-apphelp test-arp test-authz test-difxapi test-dllclass test-dllhost test-dwmthumb test-fontsub test-loadperf test-msiexec-repair test-newdev test-nsi test-pnputil test-setx test-taskmonth test-tbs test-wscript test-sn-ecma test-ncrypt-p8policy test-rohidden test-linkdel test-flatcontract test-webreqmsg test-powershellstdio test-portraitdesktop test-althook test-focusswitch test-rpcselfunreg test-ldapemptyhost test-hostroots test-nvdabindings test-wminetutils test-msiperuser test-toolkitenv test-kbdhkl test-linksize test-reole test-retom test-olelink test-stgswmr test-colorglyph test-uspfeat test-eot test-wsproto test-adoauth test-stdmarshalex test-kmount test-wicfast test-pm3 test-wmiclass test-doctypeent test-gdipprop test-ps2csa test-qcaex test-smallexp test-d3d9share test-procdpi test-stacklimit test-msirmuse test-deskread test-relocheap test-noalias test-readview test-wowstack test-rootwow test-sparse test-comuser test-pkgverb test-unknowntype test-adoodbc test-adoparam test-vbsnothing test-sysappx test-adoupdate test-adolong test-junction test-wmienum test-tom2range test-wicimageenc test-xproc-still test-ctrlrange test-wuianim test-displaysvc test-tomfont2 test-somschema test-wmipoly test-blockmap test-appxwriter test-secresults test-xproc-child test-sdbfile test-appxbundlew test-d2dblend test-txfail test-gdipemfplus test-gdipoutline test-icmdefaults test-pscff test-ttsvoice test-engpaths test-kmtqueued test-dpvsession test-tcnone test-mmiotemp test-clrtools test-acmfilter test-dnssd test-dnsupdate test-wmsynth test-clickonce test-aviclip test-restartreason test-dumpread test-ofnpreview test-monoagent test-mciconfig test-wmedia test-wmeditor test-wsdprovider test-lpcport test-wsfault test-icmadmin test-ppcontrol test-voicename test-qcaprender test-wmwriter test-wmencode test-winusbapi test-winusb test-wmtiming test-mpris test-shelllibrary test-appcmdkeys test-regnotifyapc test-iconcross test-icuconv test-connrefused test-dwmframe test-devnodeprops test-sha3 test-bcryptkdf test-schemedesktop test-aesecb test-sapiaudioout test-keynames test-unixpipe test-cmdlabelecho test-glhiddenframe test-d3d11msaaflags test-taskviewmove test-childbutton test-hlslcopyprop test-wtsunload test-glforeign test-composited test-printguard test-linkprocess test-evrpresenter test-fontsfolder test-fontsshared test-wbemrefresher test-impersonatename test-lconvwide test-fontsnoz test-bgplayer test-cffoutline test-dwoutline test-heifdecode test-dwbitmapmode test-layeredchild test-kbdcues test-rpcicarus test-svcsetstatus test-uiadisconnectall test-txdrawscroll test-noassoc test-fxinline test-wowattach test-k32gpa test-setgidtree test-hkcuwow32 test-conpty test-xmlappend test-msifont test-regexpandpair test-vcompsched

all: build

build:
	PREFIX=$(PREFIX) JOBS=$(JOBS) ./build.sh

install:
	PREFIX=$(PREFIX) JOBS=$(JOBS) DESTDIR=$(DESTDIR) DO_INSTALL=1 ./build.sh

# No user-visible "Windows" as our name (Microsoft's trademark): in the text
# our patches ADD, and in the English resources of the Wine programs and
# dialogs a user can reach (a series-applied tree, tools/lint-tree.sh).
# tools/trademark-allow.txt lists the exceptions, each with its reason.
# And every gate that runs Wine sources test/scratch-home.sh first, so no
# prefix of a gate links into the real HOME (a gate once emptied a real Desktop).
lint:
	@for f in $$(grep -l WINEPREFIX test/*.sh); do \
	    sed -n 2p "$$f" | grep -q '^\. "$$(dirname "$$0")/scratch-home.sh"$$' || \
	    { echo "$$f: line 2 must be: . \"\$$(dirname \"\$$0\")/scratch-home.sh\""; exit 1; }; done
	@T=$$(tools/lint-tree.sh) && \
	python3 tools/trademark-check.py --allow tools/trademark-allow.txt --patches patches/sg --root "$$T" \
	    --brand 'dlls/shell32/*' Wine --brand 'dlls/comdlg32/*' Wine --brand 'dlls/user32/*' Wine \
	    "$$T"/programs/*/*.rc "$$T"/dlls/shell32/*.rc "$$T"/dlls/comdlg32/*.rc "$$T"/dlls/user32/*.rc

# Runs against the *installed* tree, not the build tree, because what we ship
# is what we care about. Install first.
test:
	PREFIX=$(PREFIX) test/wow64-gate.sh

# ResourceLoader/PRI and Windows.Foundation.Uri (patches 0032, 0033), against
# the installed tree. WINGET_DIR=<unpacked winget> adds a makepri-written index.
test-resources:
	WINE=$(PREFIX)/bin/wine test/resources-gate.sh

# AppX/MSIX reading and signature trust (0034-0042); NETWORK=1 adds winget's
# real, Microsoft-signed source package.
test-appx:
	WINE=$(PREFIX)/bin/wine test/appx-gate.sh

# MSIX beyond one package (0200-0204): frameworks and the package graph,
# app execution aliases, updates, bundles' resource packages; WINGET_DIR=<a
# user-supplied winget> adds winget installing and removing an MSIX.
test-msix:
	WINE=$(PREFIX)/bin/wine test/msix-gate.sh

# A domain account's real SID (0205): tokens, files, HKCU, names, ACLs. Needs
# sudo and the second Unix user sgconf (a made-up domain record in /run).
test-domainsid:
	WINE=$(PREFIX)/bin/wine test/domainsid-gate.sh

# cmd's dir of a UNC path (0206). Needs sudo (a tmpfs share under /run).
test-uncdir:
	WINE=$(PREFIX)/bin/wine test/uncdir-gate.sh

# winex11: a program on another X display than the desktop's (0207), and the
# desktop's XRender drawing clipped by its windows (0208). Two Xvfb servers.
test-display:
	WINE=$(PREFIX)/bin/wine test/display-gate.sh

# The real winget end to end (WINGET_DIR=<a user-supplied winget>; network).
test-winget:
	WINE=$(PREFIX)/bin/wine test/winget-e2e.sh

# The Compression API (0036).
test-compress:
	WINE=$(PREFIX)/bin/wine test/compress-gate.sh

# WebView2 apps draw (0190-0191): WV2_INSTALLER=<the user's Evergreen runtime
# installer> WV2_SDK=<unpacked Microsoft.Web.WebView2 package>, or NETWORK=1.
test-webview2:
	WINE=$(PREFIX)/bin/wine test/webview2-gate.sh

# What Microsoft Edge needed (0049-0054); EDGE_MSI=<a user-supplied Edge
# enterprise MSI> also installs and runs Edge itself.
test-edge:
	WINE=$(PREFIX)/bin/wine test/edge-e2e.sh

# ClearType text and the Stained Glass scroll bars (0064-0066), by pixels.
test-theme:
	WINE=$(PREFIX)/bin/wine test/theme-gate.sh

# Cloaked windows, the primitive virtual desktops are built on (0067).
test-cloak:
	WINE=$(PREFIX)/bin/wine test/cloak-gate.sh

# Virtual desktops and Task View (0068).
test-vdesk:
	WINE=$(PREFIX)/bin/wine test/vdesk-gate.sh

# The taskbar's desktop pager, four desktops, Task View reopens (0446).
test-vdpager:
	WINE=$(PREFIX)/bin/wine test/vdpager-gate.sh

# A standard user trusts the machine's root certificates (0447).
test-userroots:
	WINE=$(PREFIX)/bin/wine test/userroots-gate.sh

# A window focused before it is shown gets the keyboard (0449).
test-focusmap:
	WINE=$(PREFIX)/bin/wine test/focusmap-gate.sh

# A folder's right-click menu: New > Folder, Shortcut, Text Document (0450).
test-newtext:
	WINE=$(PREFIX)/bin/wine test/newtext-gate.sh

# Push buttons and message boxes as Windows 10 draws them (0454).
test-flatbuttons:
	WINE=$(PREFIX)/bin/wine test/flatbuttons-gate.sh

# A grant to BUILTIN\\Users reaches the machine's users: the prefix's group (0456).
test-usersacl:
	WINE=$(PREFIX)/bin/wine test/usersacl-gate.sh

# A user's own HKCU after an elevated program wrote there, and a 32-bit
# program's HKCU\\Software is the shared key; a hive's keys are the user's (5948-5950).
test-userhive:
	WINE=$(PREFIX)/bin/wine test/userhive-gate.sh

# A key granted to BUILTIN\\Users is theirs to write from a 64-bit program's
# view of the 32-bit registry: the keys on the way are passed through (0459).
test-regwalk:
	WINE=$(PREFIX)/bin/wine test/regwalk-gate.sh

# A program's crash is told in this system's words, not Wine's (0460).
test-crashbrand:
	WINE=$(PREFIX)/bin/wine test/crashbrand-gate.sh

# Another thread's hidden window leaves the foreground where it is: Firefox
# stays full screen (0461).
test-fgthread:
	WINE=$(PREFIX)/bin/wine test/fgthread-gate.sh

# The interface fonts' names are families ("Segoe UI"), not faces' full names:
# Firefox's system-ui font (0462).
test-metricsfont:
	WINE=$(PREFIX)/bin/wine test/metricsfont-gate.sh

# The administrative tools' launchers carry icons; .msc files show mmc.exe's (0463).
test-admintoolicons:
	WINE=$(PREFIX)/bin/wine test/admintoolicons-gate.sh

# Right-clicking the taskbar opens its menu, Start the quick link menu (0464).
test-traymenu:
	WINE=$(PREFIX)/bin/wine test/traymenu-gate.sh

# On a shared prefix SYSTEM's token is elevated; a standard user's is not (0465).
test-sysrelevated:
	WINE=$(PREFIX)/bin/wine test/sysrelevated-gate.sh

# SYSTEM may change the permissions of a file another account owns: CAP_FOWNER,
# permitted only, effective for one fchmod (0466).
test-sysfowner:
	WINE=$(PREFIX)/bin/wine test/sysfowner-gate.sh

# Every user of a shared prefix has APPDATA, LOCALAPPDATA, HOMEDRIVE, HOMEPATH (0467).
test-logonvars:
	WINE=$(PREFIX)/bin/wine test/logonvars-gate.sh

# A drive's menu: Open first, Eject for discs and removable drives, no Cut/Delete (0468).
test-drivemenu:
	WINE=$(PREFIX)/bin/wine test/drivemenu-gate.sh

# The Network folder: computers, their shares, \\\\server paths (0469).
test-netplaces:
	WINE=$(PREFIX)/bin/wine test/netplaces-gate.sh

# An environment block's USERPROFILE (ProfileImagePath) and no literal %variable% (0470).
test-envblock:
	WINE=$(PREFIX)/bin/wine test/envblock-gate.sh

# A symlink opened as itself says REPARSE_POINT (0471): Office's staging.
test-reparseinfo:
	WINE=$(PREFIX)/bin/wine test/reparseinfo-gate.sh

# ShellExecute of a long URL (a mailto: with a body) reaches its handler (0472).
test-longurl:
	WINE=$(PREFIX)/bin/wine test/longurl-gate.sh

# A program's taskbar button menu: its name, Report a problem, Close (0473).
test-btnmenu:
	WINE=$(PREFIX)/bin/wine test/btnmenu-gate.sh

# Shares with a name and password: own connection first, net use /user (0474).
test-netlogon:
	WINE=$(PREFIX)/bin/wine test/netlogon-gate.sh

# msdelta refuses instead of killing Click-to-Run (0475).
test-msdelta:
	WINE=$(PREFIX)/bin/wine test/msdelta-gate.sh

# A window menu's Move and Size follow the pointer until a click (0476).
test-movesize:
	WINE=$(PREFIX)/bin/wine test/movesize-gate.sh

# Closing a desktop's last window stays on that desktop (0477).
test-vdstay:
	WINE=$(PREFIX)/bin/wine test/vdstay-gate.sh

# Our own folder, file, text file and Notepad icons (theme/icons.py, 0478).
test-icons:
	WINE=$(PREFIX)/bin/wine test/icons-gate.sh

# Save As > Desktop shows the desktop's files, not namespace links (0480).
test-savedesk:
	WINE=$(PREFIX)/bin/wine test/savedesk-gate.sh

# After a resolution change the taskbar is on the new screen, icons by the clock (0481).
test-dispchange:
	WINE=$(PREFIX)/bin/wine test/dispchange-gate.sh

# The Rounded style rounds windows' corners (0483).
test-rounded:
	WINE=$(PREFIX)/bin/wine test/rounded-gate.sh

# The Rounded style's edit boxes, list boxes and views are round too; radio buttons whole (0740).
test-roundedctl:
	WINE=$(PREFIX)/bin/wine test/roundedctl-gate.sh

# Programs pinned to the taskbar (0485).
test-taskpins:
	WINE=$(PREFIX)/bin/wine test/taskpins-gate.sh

# The Horizon and Glass looks' window frames (0742).
test-eraframes:
	WINE=$(PREFIX)/bin/wine test/eraframes-gate.sh

# The Horizon and Glass looks' controls: their colour schemes (0743, theme/eras.py).
test-eraschemes:
	WINE=$(PREFIX)/bin/wine test/eraschemes-gate.sh

# What the desktop's compositor (sg-deskcomp) is told: its picture, window shadows (0744).
test-deskprops:
	WINE=$(PREFIX)/bin/wine test/deskprops-gate.sh

# A new user's first taskbar pins, from HKLM DefaultPins (0741).
test-defaultpins:
	WINE=$(PREFIX)/bin/wine test/defaultpins-gate.sh

# 0602: pinned programs show their own icons (File Explorer: the folder).
test-pinicon:
	WINE=$(PREFIX)/bin/wine test/pinicon-gate.sh

# 0609: the desktop keeps winspool loaded (printers load once a session).
test-spooler:
	WINE=$(PREFIX)/bin/wine test/spooler-gate.sh

# 0610: where a window leaves the desktop, X shows the desktop at once.
test-desktopbg:
	WINE=$(PREFIX)/bin/wine test/desktopbg-gate.sh

# 0611: an alpha-layered popup is blended over what is under it.
test-layeredblend:
	WINE=$(PREFIX)/bin/wine test/layeredblend-gate.sh

# 0612: Linux windows' taskbar buttons show their apps' icons.
test-linuxicon:
	WINE=$(PREFIX)/bin/wine test/linuxicon-gate.sh

# 0613: New offers the file types programs register (ShellNew).
test-shellnew:
	WINE=$(PREFIX)/bin/wine test/shellnew-gate.sh

# 0615: another process's Vulkan frames in a glass popup are blended (Chrome's bubbles).
test-glassvk:
	WINE=$(PREFIX)/bin/wine test/glassvk-gate.sh

# A Vulkan glass popup keeps its see-through edge under the compositor (0753).
test-glasspixel:
	test/glasspixel-gate.sh

# The programs that start at sign-in: Run keys and Startup folders (0754).
test-signin-startup:
	WINE=$(PREFIX)/bin/wine test/signin-startup-gate.sh

# A shell started again shows the desktop in its own X window (0755).
test-shellrestart:
	WINE=$(PREFIX)/bin/wine test/shellrestart-gate.sh

# An elevated program's tray icon is in the shell's tray, and its clicks
# reach it through UIPI (0771).
test-elevtray:
	WINE=$(PREFIX)/bin/wine test/elevtray-gate.sh

# Shift+click or a middle click on a taskbar button opens another window (0773).
test-anotherwindow:
	WINE=$(PREFIX)/bin/wine test/anotherwindow-gate.sh

# A program's second thread registers window classes (0775).
test-threadclass:
	WINE=$(PREFIX)/bin/wine test/threadclass-gate.sh

# File Explorer's tabs (0774).
test-fetabs:
	WINE=$(PREFIX)/bin/wine test/fetabs-gate.sh

# bash.exe: the Linux side's bash from PowerShell or cmd (0776).
test-linuxbash:
	WINE=$(PREFIX)/bin/wine test/linuxbash-gate.sh

# A batch file is read in the console's code page (0756).
test-batchcp:
	WINE=$(PREFIX)/bin/wine test/batchcp-gate.sh

# sysdm.cpl: System Properties opens the Control Panel's own (0757).
test-sysdm:
	WINE=$(PREFIX)/bin/wine test/sysdm-gate.sh

# A Linux window's stand-in answers close and restore (0759).
test-linuxstandin:
	WINE=$(PREFIX)/bin/wine test/linuxstandin-gate.sh

# 0617: an elevated program's Start menu and desktop are all users'.
test-elevfolders:
	WINE=$(PREFIX)/bin/wine test/elevfolders-gate.sh

# 0619: the display devices a session records stay writable by its user.
test-displaycfg:
	WINE=$(PREFIX)/bin/wine test/displaycfg-gate.sh

# 0620: kernel32's process snapshot API (BleachBit's packed python312.dll).
test-pss:
	WINE=$(PREFIX)/bin/wine test/pss-gate.sh

# 0621: what pywin32 imports, exported and working (BleachBit's pythoncom).
test-pyimports:
	WINE=$(PREFIX)/bin/wine test/pyimports-gate.sh

# 0622: OWNER RIGHTS ACEs are the owner's (CPython's os.mkdir(path, 0o700)).
test-ownerrights:
	WINE=$(PREFIX)/bin/wine test/ownerrights-gate.sh

# 0624: the SYSTEM account's own processes may be traced (CAP_SYS_PTRACE when needed).
test-ptracecap:
	WINE=$(PREFIX)/bin/wine test/ptracecap-gate.sh

# 0627: a COM proxy keeps its security blanket (Omaha: Brave's installer).
test-proxyblanket:
	WINE=$(PREFIX)/bin/wine test/proxyblanket-gate.sh

# 0628: a COM local server's launcher may exit first (Omaha's ...OnDemand.exe).
test-comlauncher:
	WINE=$(PREFIX)/bin/wine test/comlauncher-gate.sh

# 0629: a cloaked window is still painted (Brave uncloaks in WM_NCPAINT).
test-cloakpaint:
	WINE=$(PREFIX)/bin/wine test/cloakpaint-gate.sh

# 0633: gdi32 exports the GPU scheduling priority class calls (OBS Studio).
test-gpupriority:
	WINE=$(PREFIX)/bin/wine test/gpupriority-gate.sh

# 0634: dbghelp skips unresolvable inline sites in step (OBS's PDBs).
test-dbghelpinline:
	WINE=$(PREFIX)/bin/wine test/dbghelpinline-gate.sh

# 0635: registry hives are fsync'd before they replace the old ones.
test-regfsync:
	WINE=$(PREFIX)/bin/wine test/regfsync-gate.sh

# 0636: shell32 and vcruntime140 versions installers look for (TortoiseGit's MSI).
test-sysversions:
	WINE=$(PREFIX)/bin/wine test/sysversions-gate.sh

# 0637: drives and discs coming and going reach File Explorer and programs.
test-drivewatch:
	WINE=$(PREFIX)/bin/wine test/drivewatch-gate.sh

# 0638: GetTempPath2 gives SYSTEM %SystemRoot%\SystemTemp\.
test-systemtemp:
	WINE=$(PREFIX)/bin/wine test/systemtemp-gate.sh

# IP_MULTICAST_IF by interface index (0639): DYMO Connect's web service.
test-mcastif:
	WINE=$(PREFIX)/bin/wine test/mcastif-gate.sh

# Print Screen goes to a screenshot tool that registers it (0640): Greenshot.
test-prtsc:
	WINE=$(PREFIX)/bin/wine test/prtsc-gate.sh

# A scanner's INF: Include/Needs and the still image class's lines (0641,
# sti.inf from 0642): the Ambir ImageScan Pro 490i.
test-stiinf:
	WINE=$(PREFIX)/bin/wine test/stiinf-gate.sh

# The USB tree scanner software walks (0643), usbscan.sys installed (0642).
test-usbtree:
	WINE=$(PREFIX)/bin/wine test/usbtree-gate.sh

# kernel32's GetProcAddress goes through kernelbase (0644): TWAINDSM's hook.
test-gpahook:
	WINE=$(PREFIX)/bin/wine test/gpahook-gate.sh

# A USB drive that arrives gets a notification that opens it (0748).
test-autoplay:
	WINE=$(PREFIX)/bin/wine test/autoplay-gate.sh

# A changed environment reaches the shell, and new programs (0749).
test-envreload:
	WINE=$(PREFIX)/bin/wine test/envreload-gate.sh

# The taskbar's Horizon and Glass looks, beside the flat default (0600).
test-taskbarlooks:
	WINE=$(PREFIX)/bin/wine test/taskbarlooks-gate.sh

# Our round and slanted shapes drawn smooth (1240): taskbar plates, tabs, chevrons.
test-smoothshapes:
	WINE=$(PREFIX)/bin/wine test/smoothshapes-gate.sh

# Our icons have every frame, 16-256 px (1241, theme/icons.py), in the sources and installed.
test-iconframes:
	WINE=$(PREFIX)/bin/wine test/iconframes-gate.sh

# XML Schema patterns with MSXML's \uXXXX escapes (0489): Office's installer.
test-xsdpattern:
	WINE=$(PREFIX)/bin/wine test/xsdpattern-gate.sh

# removeChild of a node replaceChild took out (0490): Office's installer.
test-msxml-replace:
	WINE=$(PREFIX)/bin/wine test/msxml-replace-gate.sh

# The Software Licensing client's store (0491): Office's installer.
test-sppc:
	WINE=$(PREFIX)/bin/wine test/sppc-gate.sh

# A process's activation filter (0493): Word.
test-actfilter:
	WINE=$(PREFIX)/bin/wine test/actfilter-gate.sh

# SetFileShortName (0494): a helper Word starts.
test-shortname:
	WINE=$(PREFIX)/bin/wine test/shortname-gate.sh

# The session's Linux programs' windows on the taskbar (0496).
test-xwin-taskbar:
	WINE=$(PREFIX)/bin/wine test/xwin-taskbar-gate.sh

# AVL tables, RtlIsNameInExpression, FindNextFileNameW (0498, 0499): Office.
test-avl:
	WINE=$(PREFIX)/bin/wine test/avl-gate.sh

# Elevated windows on the taskbar, minimizable (0500).
test-elevmin:
	WINE=$(PREFIX)/bin/wine test/elevmin-gate.sh

# DirectWrite: a collection's font set (0503): Office's text.
test-dwfontset:
	WINE=$(PREFIX)/bin/wine test/dwfontset-gate.sh

# The C++ library's time zone database (0504): Word.
test-tzdb:
	WINE=$(PREFIX)/bin/wine test/tzdb-gate.sh

# msvcp's _Sinh/_Cosh (0509): std::complex's sin/cos/sinh/cosh/tan -- SG
# Office's (LibreOffice for Windows) IMSIN, IMSINH ...
test-sinh:
	WINE=$(PREFIX)/bin/wine test/sinh-gate.sh

# RegRestoreKey restores binary and text hives (0506): Office's virtual registry.
test-regrestore:
	WINE=$(PREFIX)/bin/wine test/regrestore-gate.sh

# chakra.dll, the JavaScript hosting API on QuickJS (0508): Office's React Native panes.
test-chakra:
	WINE=$(PREFIX)/bin/wine test/chakra-gate.sh

# RtlValidRelativeSecurityDescriptor (0507): offreg opens Office's virtual registry hives.
test-relsd:
	WINE=$(PREFIX)/bin/wine test/relsd-gate.sh

# vcruntime140's __C_specific_handler_noexcept (0510): Word's sign-in click.
test-noexcept:
	WINE=$(PREFIX)/bin/wine test/noexcept-gate.sh

# CalculatePopupWindowPosition (0511): Word's "Sign in or create account".
test-popuppos:
	WINE=$(PREFIX)/bin/wine test/popuppos-gate.sh

# Right-click Delete goes to the Recycle Bin without a hidden prompt (0514).
test-recycle:
	WINE=$(PREFIX)/bin/wine test/recycle-gate.sh

# File Explorer's Sort, View and Refresh glyphs (0515), from a screenshot.
test-explorer-glyphs:
	WINE=$(PREFIX)/bin/wine test/explorer-glyphs-gate.sh

# Windows do not flash black when shown (0519).
test-blackflash:
	WINE=$(PREFIX)/bin/wine test/blackflash-gate.sh

# A window activated from another process (the taskbar) gets the keyboard (0527).
test-focusactivate:
	WINE=$(PREFIX)/bin/wine test/focus-activate-gate.sh

# Win32_Printer lists the printers (0870).
test-wmiprinter:
	WINE=$(PREFIX)/bin/wine test/wmiprinter-gate.sh

# Printer makers' Windows drivers (1020-1022): packages install as they
# ship, makers' own drivers print through their blits and WritePrinter;
# a PPD on our PSCRIPT5 (wineps) sends every option; HP-style PCL XL
# vector plug-ins on Unidrv.
test-drvpkg:
	WINE=$(PREFIX)/bin/wine test/drvpkg-gate.sh
test-psdriver:
	WINE=$(PREFIX)/bin/wine test/psdriver-gate.sh
test-unidrvxl:
	WINE=$(PREFIX)/bin/wine test/unidrvxl-gate.sh
test-unidrvrast:
	WINE=$(PREFIX)/bin/wine test/unidrvrast-gate.sh
test-psplugin:
	WINE=$(PREFIX)/bin/wine test/psplugin-gate.sh
test-cpsui:
	WINE=$(PREFIX)/bin/wine test/cpsui-gate.sh
test-drvui:
	WINE=$(PREFIX)/bin/wine test/drvui-gate.sh
test-netport:
	WINE=$(PREFIX)/bin/wine test/netport-gate.sh
test-printproc:
	WINE=$(PREFIX)/bin/wine test/printproc-gate.sh

# A CUPS printer's driver is named after its make and model (0871).
test-cupsmodel:
	WINE=$(PREFIX)/bin/wine test/cupsmodel-gate.sh

# A printer added to CUPS during a session reaches Windows programs (0873).
test-cupsrefresh:
	WINE=$(PREFIX)/bin/wine test/cupsrefresh-gate.sh

# A standard user reads a device's own key (0872).
test-devkeyread:
	WINE=$(PREFIX)/bin/wine test/devkeyread-gate.sh

# A standard user reads a device's driver key (0580).
test-drvkey:
	WINE=$(PREFIX)/bin/wine test/drvkey-gate.sh

# A standard user's audio endpoints have properties (0581).
test-mmdevkey:
	WINE=$(PREFIX)/bin/wine test/mmdevkey-gate.sh

# The file dialogs as Windows shows them (0582).
test-ofn:
	WINE=$(PREFIX)/bin/wine test/ofn-gate.sh

# A shell folder is never linked to itself (0583).
test-selflink:
	WINE=$(PREFIX)/bin/wine test/selflink-gate.sh

# Starting a Unix program leaks nothing (0584).
test-unixexec:
	WINE=$(PREFIX)/bin/wine test/unixexec-gate.sh

# Machine components found by an account that may only read the Installer
# keys (0701): Word's "(6)" after Office's integrator ran.
test-msireadonly:
	WINE=$(PREFIX)/bin/wine test/msireadonly-gate.sh

# Wine's windows come in front of Linux programs' windows (0700).
test-desktopfront:
	WINE=$(PREFIX)/bin/wine test/desktopfront-gate.sh

# The Linux programs' windows in Task View (0585).
test-taskview-linux:
	WINE=$(PREFIX)/bin/wine test/taskview-linux-gate.sh

# Another process renders into a window with Vulkan (0586).
test-xproc-vk:
	WINE=$(PREFIX)/bin/wine test/xproc-vk-gate.sh

# A window is not shown black before its program paints it (0587).
test-firstpaint:
	WINE=$(PREFIX)/bin/wine test/firstpaint-gate.sh

# A folder the user may not write to asks for an administrator (0589).
test-fileopadmin:
	WINE=$(PREFIX)/bin/wine test/fileopadmin-gate.sh

# A package for all users is installed as an administrator, its shortcuts for all (0590).
test-msiallusers:
	WINE=$(PREFIX)/bin/wine test/msiallusers-gate.sh

# A program's own right-click verbs show their names (0591).
test-verblabel:
	WINE=$(PREFIX)/bin/wine test/verblabel-gate.sh

# A window stays responsive while it copies (0592).
test-fileopresponsive:
	WINE=$(PREFIX)/bin/wine test/fileopresponsive-gate.sh

# Map network drive and Disconnect network drive (0593).
test-mapdrive:
	WINE=$(PREFIX)/bin/wine test/mapdrive-gate.sh

# Open with (0594).
test-openwith:
	WINE=$(PREFIX)/bin/wine test/openwith-gate.sh

# A big folder shows at once (0595).
test-folderspeed:
	WINE=$(PREFIX)/bin/wine test/folderspeed-gate.sh

# Create shortcut (0596).
test-shortcut:
	WINE=$(PREFIX)/bin/wine test/shortcut-gate.sh

# The taskbar is above the windows, behind a full-screen one (0597).
test-topbar:
	WINE=$(PREFIX)/bin/wine test/topbar-gate.sh

# An installer's feature tree shows its states, description and size (0598).
test-msiseltree:
	WINE=$(PREFIX)/bin/wine test/msiseltree-gate.sh

# File Explorer's status bar follows the view (0599).
test-statuscount:
	WINE=$(PREFIX)/bin/wine test/statuscount-gate.sh

# The folder view follows its folder (0601).
test-folderwatch:
	WINE=$(PREFIX)/bin/wine test/folderwatch-gate.sh

# Task View's windows fly into place (0520).
test-taskview-anim:
	WINE=$(PREFIX)/bin/wine test/taskview-anim-gate.sh

# Switching virtual desktops slides (0521).
test-desktop-slide:
	WINE=$(PREFIX)/bin/wine test/desktop-slide-gate.sh

# The Web Account Manager: a work account's sign-in through sg-wam-msal (0512,
# 1479), with a stub program standing in for it.
test-webauthcore:
	WINE=$(PREFIX)/bin/wine test/webauthcore-gate.sh

# wininet accepts the listen timeout Office's HTTP client sets (1477).
test-wininet-listentimeout:
	WINE=$(PREFIX)/bin/wine test/wininet-listentimeout-gate.sh

# The online identity API's ticket request is answered (1478).
test-onlineid-ticket:
	WINE=$(PREFIX)/bin/wine test/onlineid-ticket-gate.sh

# RichEdit's ITextServices2 and ITextDocument2 (1490): Word's React Native
# start screen ended in a fail-fast without them.
test-richtxsrv2:
	WINE=$(PREFIX)/bin/wine test/richtxsrv2-gate.sh

# A Direct2D bitmap shared from another bitmap can be drawn (1491): Word's
# ribbon font name and size boxes were black.
test-d2dsharedview:
	WINE=$(PREFIX)/bin/wine test/d2dsharedview-gate.sh

# combase's RoTransformError and RoOriginateErrorW return (1492): Word ended
# a minute after opening a document.
test-rotransformerror:
	WINE=$(PREFIX)/bin/wine test/rotransformerror-gate.sh

# Other processes' CPU times and cycle counts (1600).
test-cputime:
	WINE=$(PREFIX)/bin/wine test/cputime-gate.sh

# The file information classes Windows has (1601).
test-fileinfo:
	WINE=$(PREFIX)/bin/wine test/fileinfo-gate.sh

# Process and thread settings kept and read back (1602).
test-procsettings:
	WINE=$(PREFIX)/bin/wine test/procsettings-gate.sh

# CryptProtectMemory / RtlEncryptMemory encrypt (1603).
test-memprotect:
	WINE=$(PREFIX)/bin/wine test/memprotect-gate.sh

# Display affinity, user object security, power notifications (1604).
test-userapis:
	WINE=$(PREFIX)/bin/wine test/userapis-gate.sh

# Precise interrupt time, file streams, fail-fast, drivers, page files (1605).
test-kbmisc:
	WINE=$(PREFIX)/bin/wine test/kbmisc-gate.sh

# CPU sets, NUMA nodes, ideal processors, priority boost (1606).
test-cpusets:
	WINE=$(PREFIX)/bin/wine test/cpusets-gate.sh

# Error report registrations (1607).
test-werreg:
	WINE=$(PREFIX)/bin/wine test/werreg-gate.sh

# Device family version, ETW provider information, other threads' layout (1608).
test-devfamily:
	WINE=$(PREFIX)/bin/wine test/devfamily-gate.sh

# SetEntriesInAclW builds the ACL as Windows does (1609).
test-aclentries:
	WINE=$(PREFIX)/bin/wine test/aclentries-gate.sh

# A new RichEdit has no placeholder text (1610): Word crashed on exit.
test-richinit:
	WINE=$(PREFIX)/bin/wine test/richinit-gate.sh

# Buffered painting parameters, clear, set alpha (1611).
test-bufpaint:
	WINE=$(PREFIX)/bin/wine test/bufpaint-gate.sh

# dwmapi accent colour, attributes, blur behind (1612).
test-dwmattrs:
	WINE=$(PREFIX)/bin/wine test/dwmattrs-gate.sh

# GetFileMUIPath (1613).
test-muipath:
	WINE=$(PREFIX)/bin/wine test/muipath-gate.sh

# SHGetStockIconInfo flags (1614).
test-stockicon:
	WINE=$(PREFIX)/bin/wine test/stockicon-gate.sh

# EnumJobs lists a printer's queue; pages are counted (1615).
test-enumjobs:
	WINE=$(PREFIX)/bin/wine test/enumjobs-gate.sh

# ReplaceFile merges; power information; small stubs (1616).
test-replacefile:
	WINE=$(PREFIX)/bin/wine test/replacefile-gate.sh

# EnumDependentServices lists the dependents (1617).
test-svcdepend:
	WINE=$(PREFIX)/bin/wine test/svcdepend-gate.sh

# InitiateSystemShutdown/InitiateShutdown, abortably (1618).
test-sysshutdown:
	WINE=$(PREFIX)/bin/wine test/sysshutdown-gate.sh

# Privilege display names, Safer levels, security down a tree (1619).
test-advsec:
	WINE=$(PREFIX)/bin/wine test/advsec-gate.sh

# shcore: explicit AppUserModelID, scale factors and registrations, IsOS (1620).
test-shcorebits:
	WINE=$(PREFIX)/bin/wine test/shcorebits-gate.sh

test-fontquery:
	WINE=$(PREFIX)/bin/wine test/fontquery-gate.sh

test-guithread:
	WINE=$(PREFIX)/bin/wine test/guithread-gate.sh

test-ipnotify:
	WINE=$(PREFIX)/bin/wine test/ipnotify-gate.sh

test-apprestart:
	WINE=$(PREFIX)/bin/wine test/apprestart-gate.sh

test-minidumpx:
	WINE=$(PREFIX)/bin/wine test/minidumpx-gate.sh

test-joblimits:
	WINE=$(PREFIX)/bin/wine test/joblimits-gate.sh

test-fmtsupport:
	WINE=$(PREFIX)/bin/wine test/fmtsupport-gate.sh

test-printnotify:
	WINE=$(PREFIX)/bin/wine test/printnotify-gate.sh

test-diskinfo:
	WINE=$(PREFIX)/bin/wine test/diskinfo-gate.sh

test-perfcount:
	WINE=$(PREFIX)/bin/wine test/perfcount-gate.sh

test-taskdisp:
	WINE=$(PREFIX)/bin/wine test/taskdisp-gate.sh

test-taskinfo:
	WINE=$(PREFIX)/bin/wine test/taskinfo-gate.sh

test-taskxml:
	WINE=$(PREFIX)/bin/wine test/taskxml-gate.sh

test-bitsjob:
	WINE=$(PREFIX)/bin/wine test/bitsjob-gate.sh

test-bitsmore:
	WINE=$(PREFIX)/bin/wine test/bitsmore-gate.sh

test-mmcss:
	WINE=$(PREFIX)/bin/wine test/mmcss-gate.sh

test-devtree:
	WINE=$(PREFIX)/bin/wine test/devtree-gate.sh

test-pinterfaceiid:
	WINE=$(PREFIX)/bin/wine test/pinterfaceiid-gate.sh

test-taskprogress:
	WINE=$(PREFIX)/bin/wine test/taskprogress-gate.sh

test-winappid:
	WINE=$(PREFIX)/bin/wine test/winappid-gate.sh

test-childpolicy:
	WINE=$(PREFIX)/bin/wine test/childpolicy-gate.sh

test-smallstubs2:
	WINE=$(PREFIX)/bin/wine test/smallstubs2-gate.sh

test-asynclock:
	WINE=$(PREFIX)/bin/wine test/asynclock-gate.sh

test-uilangs:
	WINE=$(PREFIX)/bin/wine test/uilangs-gate.sh

test-shellapp:
	WINE=$(PREFIX)/bin/wine test/shellapp-gate.sh

test-shelldisp:
	WINE=$(PREFIX)/bin/wine test/shelldisp-gate.sh

test-wshshell:
	WINE=$(PREFIX)/bin/wine test/wshshell-gate.sh

test-textstreams:
	WINE=$(PREFIX)/bin/wine test/textstreams-gate.sh

test-fsoitems:
	WINE=$(PREFIX)/bin/wine test/fsoitems-gate.sh

test-wshnet:
	WINE=$(PREFIX)/bin/wine test/wshnet-gate.sh

test-vbsbuiltins:
	WINE=$(PREFIX)/bin/wine test/vbsbuiltins-gate.sh

test-vbsdyncode:
	WINE=$(PREFIX)/bin/wine test/vbsdyncode-gate.sh

test-jsstubs:
	WINE=$(PREFIX)/bin/wine test/jsstubs-gate.sh

test-nsplookup:
	WINE=$(PREFIX)/bin/wine test/nsplookup-gate.sh

test-wscatalog:
	WINE=$(PREFIX)/bin/wine test/wscatalog-gate.sh

test-gdipquality:
	WINE=$(PREFIX)/bin/wine test/gdipquality-gate.sh

test-hotkeyexec:
	WINE=$(PREFIX)/bin/wine test/hotkeyexec-gate.sh

# A locale's sort looked up re-entrantly (0513): App-V's registry hooks.
test-sortreenter:
	WINE=$(PREFIX)/bin/wine test/sortreenter-gate.sh

# cldapi.dll (Cloud Files API) and cryptxml.dll (XML digital signatures),
# the two modules Microsoft OneDrive's sync client imports that Wine lacked;
# without them OneDrive.exe cannot load its sync stack (0518).
test-onedrive-dlls:
	WINE=$(PREFIX)/bin/wine test/onedrive-dlls-gate.sh

# Service trigger info, ChangeServiceConfig2/QueryServiceConfig2 at
# SERVICE_CONFIG_TRIGGER_INFO: OneDrive's per-machine install registers its
# Updater Service with one (0523).
test-svctrigger:
	WINE=$(PREFIX)/bin/wine test/svctrigger-gate.sh

# UISettings' values and Changed events, CoreWindow.GetForCurrentThread():
# OneDrive ended at start on their stubs (0524).
test-uisettings:
	WINE=$(PREFIX)/bin/wine test/uisettings-gate.sh

# Shell_NotifyIconGetRect: where a notification-area icon is (0525).
test-notifyiconrect:
	WINE=$(PREFIX)/bin/wine test/notifyiconrect-gate.sh

# GetShellWindow() on the shell's desktop is explorer's (0526).
test-shellwindow:
	WINE=$(PREFIX)/bin/wine test/shellwindow-gate.sh

# One Service Control Manager: a second services.exe leaves (0530).
test-scmsingle:
	WINE=$(PREFIX)/bin/wine test/scmsingle-gate.sh

# X stacking follows Wine's Z order in a virtual desktop (0531).
test-xstack:
	WINE=$(PREFIX)/bin/wine test/xstack-gate.sh

# Wallpaper in any format, Windows' styles (0069).
test-wallpaper:
	WINE=$(PREFIX)/bin/wine test/wallpaper-gate.sh

# Dark mode: the Light style's Dark scheme, followed live by every program;
# dark title bars (DWMWA_USE_IMMERSIVE_DARK_MODE); the taskbar's Windows mode (0160-0163).
test-darkmode:
	WINE=$(PREFIX)/bin/wine test/darkmode-gate.sh

# A gate's prefix links its user folders into the gate's HOME, never the real one.
test-scratch-home:
	WINE=$(PREFIX)/bin/wine test/scratch-home-gate.sh

# The Run dialog: in front, typed into at once, our words and icon (0266).
test-rundialog:
	WINE=$(PREFIX)/bin/wine test/rundialog-gate.sh

# File Explorer follows the app mode, in Large icons and Details (0264, 0265).
test-explorer-dark:
	WINE=$(PREFIX)/bin/wine test/explorer-dark-gate.sh

# The taskbar honours Settings > Personalization > Taskbar (0164).
test-taskbar:
	WINE=$(PREFIX)/bin/wine test/taskbar-gate.sh

# The user's regional format is their choice (0168).
test-region:
	WINE=$(PREFIX)/bin/wine test/region-gate.sh

# A standard user's shell folders (0070).
test-shellfolders:
	WINE=$(PREFIX)/bin/wine test/shellfolders-gate.sh

# The Windows key's shortcuts (0071).
test-shellkeys:
	WINE=$(PREFIX)/bin/wine test/shellkeys-gate.sh

# control.exe follows App Paths (0072).
test-control:
	WINE=$(PREFIX)/bin/wine test/control-gate.sh

# calc/mspaint/snippingtool launchers, taskmgr/wmplayer hand-off, Win+Shift+S, Ctrl+Shift+Esc (0120-0123).
test-handoff:
	WINE=$(PREFIX)/bin/wine test/handoff-gate.sh

# A named pipe server impersonates its client, not itself (0140); needs a
# second Unix user (SG_OTHER, default sgconf) and passwordless sudo -u to it.
test-pipeimp:
	WINE=$(PREFIX)/bin/wine test/pipeimp-gate.sh

# A wineserver started on its own raises its open-files limit (0249).
test-serverfds:
	WINE=$(PREFIX)/bin/wine test/serverfds-gate.sh

# ExitWindowsEx shuts down and restarts the PC through sg-settingsctl (0241).
test-shutdown:
	WINE=$(PREFIX)/bin/wine test/shutdown-gate.sh

# A standard user's programs read the display configuration back (0242); a
# second Unix user as for test-pipeimp.
test-dispcfg:
	WINE=$(PREFIX)/bin/wine test/dispcfg-gate.sh

# A standard user's time zone and locale lookups (0243).
test-userlocale:
	WINE=$(PREFIX)/bin/wine test/userlocale-gate.sh

# A sandboxed program's own desktop comes up; nothing left spinning (0247).
test-sandboxdesk:
	WINE=$(PREFIX)/bin/wine test/sandboxdesk-gate.sh

# A requireAdministrator program: ERROR_ELEVATION_REQUIRED, ShellExecute asks
# the elevation broker (a stand-in here) (0259).
test-elevreq:
	WINE=$(PREFIX)/bin/wine test/elevreq-gate.sh

# The SCM checks who asks (0141); a second Unix user as for test-pipeimp.
test-scm-access:
	WINE=$(PREFIX)/bin/wine test/scm-access-gate.sh

# A service's own security descriptor, sc sdshow/sdset (0185); second Unix user as above.
test-scm-sd:
	WINE=$(PREFIX)/bin/wine test/scm-sd-gate.sh

# Audit events from the Linux side reach the Security log (0187); second Unix user as above.
test-audit:
	WINE=$(PREFIX)/bin/wine test/audit-gate.sh

# The administrative tools' Windows names: mmc/eventvwr/resmon/cleanmgr/msinfo32
# launchers, the .msc files and association, Start menu shortcuts, Win+X (0142, 0145);
# lusrmgr.msc/fsmgmt.msc and msinfo32 /report waiting for the file (0186).
test-admintools:
	WINE=$(PREFIX)/bin/wine test/admintools-gate.sh

# Magnifier and the On-Screen Keyboard: magnify.exe/osk.exe launchers, explorer's
# Win+Plus/Win+Minus/Win+Esc/Win+Ctrl+O (0181), appbars and the work area (0182).
test-a11y:
	WINE=$(PREFIX)/bin/wine test/a11y-gate.sh

# The X keyboard layout's keys: scan codes, AltGr, following a switch (0250-0251).
test-kbdlayout:
	WINE=$(PREFIX)/bin/wine test/kbdlayout-gate.sh

# The event log (0143, 0144): the service, the API, who may read Security;
# needs a second Unix user (SG_OTHER, default sgconf) and sudo -u to it.
test-eventlog:
	WINE=$(PREFIX)/bin/wine test/eventlog-gate.sh

# fontview.exe, the Fonts folder, per-user fonts (HKCU Fonts), shell: URLs (0183).
test-fonts:
	WINE=$(PREFIX)/bin/wine test/fonts-gate.sh

# A call that unregisters its own RPC interface returns (5938).
test-rpcselfunreg:
	WINE=$(PREFIX)/bin/wine test/rpcselfunreg-gate.sh

# ldap_init with no host name opens the default host (5939).
test-ldapemptyhost:
	WINE=$(PREFIX)/bin/wine test/ldapemptyhost-gate.sh

# An empty machine Root store holds the host's roots (5940).
test-hostroots:
	WINE=$(PREFIX)/bin/wine test/hostroots-gate.sh

# The functions NVDA binds at start are there (5941).
test-nvdabindings:
	WINE=$(PREFIX)/bin/wine test/nvdabindings-gate.sh

# What .NET's System.Management calls for WMI (5943).
test-wminetutils:
	WINE=$(PREFIX)/bin/wine test/wminetutils-gate.sh

# Startup items disabled in Task Manager (0125).
test-startup:
	WINE=$(PREFIX)/bin/wine test/startup-gate.sh

# PrintWindow of another process's window (0076).
test-printwindow:
	WINE=$(PREFIX)/bin/wine test/printwindow-gate.sh

# WinSta0\Default is the user's desktop (0080).
test-default-desktop:
	WINE=$(PREFIX)/bin/wine test/default-desktop-gate.sh

# Real applications, installed and run (network, ~1 h; not in `make test`).
test-compat:
	SG_DEFAULTS=$${SG_DEFAULTS:-../sg-shell/theme} WINE=$(PREFIX)/bin/wine test/compat/run.sh

# Native stdio, ipconfig and netsh on sg-netctl (0078-0079).
test-netsh:
	WINE=$(PREFIX)/bin/wine test/netsh-gate.sh

# VPN connections through RAS: rasapi32 and rasdial on sg-netctl (0400).
test-rasdial:
	WINE=$(PREFIX)/bin/wine test/rasdial-gate.sh

# A task dialog is made of comctl32 6's controls, manifest or not (0401).
test-taskdialog:
	WINE=$(PREFIX)/bin/wine test/taskdialog-gate.sh

# Transforms made here and deployed with msiexec TRANSFORMS= (0402).
test-msitransform:
	WINE=$(PREFIX)/bin/wine test/msitransform-gate.sh

# Console programs with no terminal get a console when they ask (0403).
test-shellconsole:
	WINE=$(PREFIX)/bin/wine test/shellconsole-gate.sh

# PowerShell script signatures; the certificate stores (0404, 0405).
test-pssig:
	WINE=$(PREFIX)/bin/wine test/pssig-gate.sh

# robocopy (0406).
test-robocopy:
	WINE=$(PREFIX)/bin/wine test/robocopy-gate.sh

# JScript GetObject; FileSystemObject with an empty path (0407).
test-jscript:
	WINE=$(PREFIX)/bin/wine test/jscript-gate.sh

# The shell's desktop icons (0090).
test-desktop-icons:
	WINE=$(PREFIX)/bin/wine test/desktop-icons-gate.sh

# The desktop: watched Desktop folders, selection, its menus, Alt+F4 asks (0261, 0262).
test-desktop:
	WINE=$(PREFIX)/bin/wine test/desktop-gate.sh

# Selecting and dragging: the desktop's rubber band, icons dragged on the desktop,
# into folders and File Explorer; File Explorer's rubber band and drags (0630-0632).
test-drag:
	WINE=$(PREFIX)/bin/wine test/drag-gate.sh

test-deskhover:
	WINE=$(PREFIX)/bin/wine test/deskhover-gate.sh

# the desktop's icons after an erase-only repaint, the shell's start after a look change (1127)
test-deskerase:
	WINE=$(PREFIX)/bin/wine test/deskerase-gate.sh

# a thread woken from an idle wait is not taken for hung (1128)
test-hungwake:
	WINE=$(PREFIX)/bin/wine test/hungwake-gate.sh

# Save As takes the name as typed: the suggestion selected, a typed type kept (1129)
test-savename:
	WINE=$(PREFIX)/bin/wine test/savename-gate.sh

# a new display scale while programs run; an appbar docks at once (1120-1126)
test-dpifollow:
	WINE=$(PREFIX)/bin/wine test/dpifollow-gate.sh

# File Explorer: layout, This PC, search, address bar, file operations (0110-0112).
test-explorer:
	WINE=$(PREFIX)/bin/wine test/explorer-gate.sh

# File Explorer round 2: thumbnails, icon sizes, tiles, groups, panes, Quick
# access, Send to, type names, drops on the navigation pane (0150-0157).
# SGZIP=<sg-shell's sg-zip64.exe> for Send to > Compressed (zipped) Folder.
test-explorer2:
	WINE=$(PREFIX)/bin/wine test/explorer2-gate.sh

# File Explorer round 3: video and PDF thumbnails, the thumbnail cache, picture
# and video details, Quick access remove and pin reordering (0244-0246).
# SG_PDF_HELPER=<sg-session's bin/sg-pdf> if it is not installed.
test-explorer3:
	WINE=$(PREFIX)/bin/wine test/explorer3-gate.sh

# A self-managed stack grows read-write; SD-less objects have an owner (0081, 0082).
test-stackgrow:
	WINE=$(PREFIX)/bin/wine test/stackgrow-gate.sh

test-objowner:
	WINE=$(PREFIX)/bin/wine test/objowner-gate.sh

# Symbolic links and junctions are real (0360): the EA app's installer.
test-symlink:
	WINE=$(PREFIX)/bin/wine test/symlink-gate.sh

# A program's compatibility settings apply when it starts (0420): the
# Environment, LaunchArgs and Version the Compatibility tab stores. RUNASADMIN
# is in test-elevreq (it needs a user who is not elevated).
test-appcompat:
	WINE=$(PREFIX)/bin/wine test/appcompat-gate.sh

# ucrtbase imaxdiv and the wide intmax conversions exist (0421): 64/32-bit,
# ucrtbase, msvcr120, msvcr120_app.
test-crtimax:
	WINE=$(PREFIX)/bin/wine test/crtimax-gate.sh

# A named stream (file:Zone.Identifier) is refused as on FAT, not made a
# second file; file::$$DATA is the file (0422).
test-streams:
	WINE=$(PREFIX)/bin/wine test/streams-gate.sh

# GetCurrentApplicationUserModelId / GetApplicationUserModelId (0423):
# Thunderbird 128 died on the missing export.
test-aumid:
	WINE=$(PREFIX)/bin/wine test/aumid-gate.sh

# IDXGIKeyedMutex on a SHARED_KEYEDMUTEX texture with Wine's own d3d11
# (0424). Needs Xvfb.
test-keyedmutex:
	WINE=$(PREFIX)/bin/wine test/keyedmutex-gate.sh

# What Zoom and Teams need at start: SetThreadpoolTimerEx/WaitEx,
# Windows.ApplicationModel.LimitedAccessFeatures (0425-0426).
test-meetings:
	WINE=$(PREFIX)/bin/wine test/meetings-gate.sh

# Direct2D geometries: area, length, containment, relations, arcs, and a
# command list's bounds (0427); Paint.NET brush masks (0429, 0430). Needs Xvfb.
test-d2dgeom:
	WINE=$(PREFIX)/bin/wine test/d2dgeom-gate.sh

# msvcp140's mutex and condition variable in the layout Visual Studio 2022
# 17.10+ constructs inline (0428).
test-stlmtx:
	WINE=$(PREFIX)/bin/wine test/stlmtx-gate.sh

# Image pages shared between processes when sections are not page-aligned in
# the file (0431: Chromium/Electron DLLs); huge PAGE_NOACCESS reservations cost
# no memory (0432: V8/PartitionAlloc cages).
test-imgshare:
	WINE=$(PREFIX)/bin/wine test/imgshare-gate.sh

test-bigreserve:
	WINE=$(PREFIX)/bin/wine test/bigreserve-gate.sh

# NtQueryWnfStateData: Thunderbird's new-mail notification (0433).
test-wnf:
	WINE=$(PREFIX)/bin/wine test/wnf-gate.sh

# AppContainer SID names: Chrome's sandbox quit the browser without them (0434).
test-appcontainer:
	WINE=$(PREFIX)/bin/wine test/appcontainer-gate.sh

# Sheet-of-glass windows keep their alpha: Firefox popups had black frames (0435).
test-glass:
	WINE=$(PREFIX)/bin/wine test/glass-gate.sh

# 0605: a composition swap chain with alpha shows its alpha (Chrome's menus).
test-dcompalpha:
	WINE=$(PREFIX)/bin/wine test/dcompalpha-gate.sh

# Toast notifications and their XML documents (0436-0437): Firefox, Chrome,
# Electron and .NET programs show them; clicks and dismissals reach the program.
test-toast:
	WINE=$(PREFIX)/bin/wine test/toast-gate.sh

# shutdown.exe (0438): /s /r /p /l /t /c, as scripts and the Ctrl+Alt+Del screen use it.
test-shutdownexe:
	WINE=$(PREFIX)/bin/wine test/shutdownexe-gate.sh

# Signing out ends the shell, so the session ends (0439). Needs Xvfb.
test-logoff:
	WINE=$(PREFIX)/bin/wine test/logoff-gate.sh

# cmd's banner and console title (0440).
test-cmdbanner:
	WINE=$(PREFIX)/bin/wine test/cmdbanner-gate.sh

# The default window icon is our mark, not Wine's glass (0441).
test-winlogo:
	WINE=$(PREFIX)/bin/wine test/winlogo-gate.sh

# A changed tray icon does not show the old one through it (0442). Needs Xvfb.
test-trayicon:
	WINE=$(PREFIX)/bin/wine test/trayicon-gate.sh

# The Desktop namespace has no "/" (0443).
test-nsroot:
	WINE=$(PREFIX)/bin/wine test/nsroot-gate.sh

# A battery reporting energy reads right (0445). Needs sudo -n (a mount namespace).
test-battery:
	WINE=$(PREFIX)/bin/wine test/battery-gate.sh

# Shared D3D11 textures under DXVK do not crash (0361): GOG Galaxy, browser
# engines. Needs DXVK_DIR (a DXVK build with x64/d3d11.dll) and Xvfb.
test-d3dshared:
	WINE=$(PREFIX)/bin/wine test/d3dshared-gate.sh

# Notepad, our editor, for everything that runs notepad.exe (0100, 0101).
test-notepad:
	WINE=$(PREFIX)/bin/wine test/notepad-gate.sh

# Notepad's dark menu bar (0188) and Page Setup's header and footer (0100);
# needs cups-daemon (the gate runs a private cupsd for a test printer).
test-notepad-print:
	WINE=$(PREFIX)/bin/wine test/notepad-print-gate.sh

# Notepad's minimap (0100).
test-notepad-minimap:
	WINE=$(PREFIX)/bin/wine test/notepad-minimap-gate.sh

# A pipe end reports how much it can write (0083).
test-pipequota:
	WINE=$(PREFIX)/bin/wine test/pipequota-gate.sh

# What developer tools need (0320 on): the console code pages of a program
# whose output is redirected (dotnet CLI).
test-devtools:
	WINE=$(PREFIX)/bin/wine test/devtools-gate.sh

# What Telegram and Signal need of WinRT at start: ApiInformation's answers
# (0390), Windows.Storage.Streams' DataWriter and in-memory streams (0391).
test-winrtstreams:
	WINE=$(PREFIX)/bin/wine test/winrtstreams-gate.sh

# DwmFlush waits for the next vertical blank: Firefox's vsync (0170).
test-vsync:
	WINE=$(PREFIX)/bin/wine test/vsync-gate.sh

# The pinned Firefox shows a page with its GPU process and exits (0170; the
# compat suite's cached installer, network once).
test-firefox:
	WINE=$(PREFIX)/bin/wine test/firefox-e2e.sh

# Characters beyond the BMP (emoji) in GDI text (0171).
test-astral:
	WINE=$(PREFIX)/bin/wine test/astral-gate.sh

# The dynamic time zone conversions (0172).
test-tzex:
	WINE=$(PREFIX)/bin/wine test/tzex-gate.sh

# Windows.System.DispatcherQueue (0174).
test-dispatcherq:
	WINE=$(PREFIX)/bin/wine test/dispatcherq-gate.sh

# Windows 10 22H2; DXGIDeclareAdapterRemovalSupport (0175, 0176).
test-winver:
	WINE=$(PREFIX)/bin/wine test/winver-gate.sh

# DirectWrite finds the font GDI uses for a substituted name (0177).
test-dwlogfont:
	WINE=$(PREFIX)/bin/wine test/dwlogfont-gate.sh

# HKEY_CLASSES_ROOT is the merged view; the user's choices (0178, 0179).
# A shared prefix with a second Unix user (SG_OTHER, default sgconf) when there is one.
test-hkcr:
	WINE=$(PREFIX)/bin/wine test/hkcr-gate.sh

# The time zone and the clock through sg-admind (0221; SG_ADMIND= sg-shell's).
test-tzset:
	WINE=$(PREFIX)/bin/wine test/tzset-gate.sh

# DirectWrite hints a glyph at the size it is drawn at: cairo/GTK text (0222).
test-dwscale:
	WINE=$(PREFIX)/bin/wine test/dwscale-gate.sh

# Direct2D effects and contexts, DXGI's WARP adapter, shader reflection's
# feature level, Windows Animation, effect bounds, geometry combination and
# widening, drawing effects and command lists, as Paint.NET uses them
# (0223-0229).
# Direct2D layers, primitive blends, DrawImage composite modes (0340);
# effect graphs as Paint.NET builds them (0341); a display's colour
# capabilities (0342). Xvfb.
test-d2dlayer:
	WINE=$(PREFIX)/bin/wine test/d2dlayer-gate.sh

test-d2dsvg:
	WINE=$(PREFIX)/bin/wine test/d2dsvg-gate.sh

test-kbchar:
	WINE=$(PREFIX)/bin/wine test/kbchar-gate.sh

test-d2dgraph:
	WINE=$(PREFIX)/bin/wine test/d2dgraph-gate.sh

test-advcolor:
	WINE=$(PREFIX)/bin/wine test/advcolor-gate.sh

test-d2dfx:
	WINE=$(PREFIX)/bin/wine test/d2dfx-gate.sh

# Windows.UI.Composition (compositor, visuals, brushes, drawing surfaces,
# desktop window targets), ID3D11Device5/ID3D11DeviceContext4/IDXGIDevice4,
# WIC half-float formats, window feedback settings (0229-0234; 0229's
# Direct2D drawing is in test-d2dfx).
test-wuicomp:
	WINE=$(PREFIX)/bin/wine test/wuicomp-gate.sh

# What popular installers need: a COM service by a long name with its
# parameters, key DACLs through write handles, [string] typelib marshalling,
# internet shortcuts, process group affinity (0235-0239).
test-appfix:
	WINE=$(PREFIX)/bin/wine test/appfix-gate.sh

# What Discord and Spotify need at start: the SSL policy's ignore-unknown-
# revocation flags, ProcessHandleTable/ProcessHandleCount (64- and 32-bit),
# the shell's tray settings key, DecryptMessage's missing count (0380-0383).
test-commapps:
	WINE=$(PREFIX)/bin/wine test/commapps-gate.sh

deb:
	dpkg-buildpackage -us -uc -b

deps:
	sudo apt-get build-dep -y wine
	sudo apt-get install -y gcc-mingw-w64-i686 gcc-mingw-w64-x86-64 \
	                        mingw-w64-tools libsane-dev \
	                        librsvg2-bin icoutils imagemagick

clean:
	rm -rf build/obj

distclean:
	rm -rf build

# Office and documents compat round (0300-0304).
test-dcrt-print:
	WINE=$(PREFIX)/bin/wine test/dcrt-print-gate.sh
test-powershell:
	WINE=$(PREFIX)/bin/wine test/powershell-gate.sh
test-cryptbase:
	WINE=$(PREFIX)/bin/wine test/cryptbase-gate.sh
test-msica:
	WINE=$(PREFIX)/bin/wine test/msica-gate.sh
test-office-docs:
	WINE=$(PREFIX)/bin/wine test/office-docs-gate.sh

test-iconhandler:
	WINE=$(PREFIX)/bin/wine test/iconhandler-gate.sh

test-gdipfamily:
	WINE=$(PREFIX)/bin/wine test/gdipfamily-gate.sh

test-dirshare:
	WINE=$(PREFIX)/bin/wine test/dirshare-gate.sh

# A Linux program drawing its own title bar gets a frame without one, moves
# and sizes it itself, and a new Linux window is framed soon (0803).
test-ownframe:
	WINE=$(PREFIX)/bin/wine test/ownframe-gate.sh

# A click on a maximized own-title-bar Linux program leaves the pointer free (0818).
test-embedmaxgrab:
	WINE=$(PREFIX)/bin/wine test/embedmaxgrab-gate.sh

# A click in the notification centre reaches the program that sent it
# (0819-0821): its COM activator, its toast's Activated event, its tray icon.
test-toastactivate:
	WINE=$(PREFIX)/bin/wine test/toastactivate-gate.sh

# The .NET Framework 3.5 as an optional feature (0827-0829): WMI's
# Win32_OptionalFeature, dism /get-featureinfo /enable-feature, fondue.exe.
test-netfx3:
	WINE=$(PREFIX)/bin/wine test/netfx3-gate.sh

# A share's folder read and shown at once (0822-0825): no request per file,
# volume information, drawn once with its icons, a crashed File Explorer
# forgotten, network drives not polled. Needs sudo (a stand-in share).
test-smbshare:
	WINE=$(PREFIX)/bin/wine test/smbshare-gate.sh
test-smbview:
	WINE=$(PREFIX)/bin/wine test/smbview-gate.sh

# NET USE lists the connections; network drives named for their folders
# (0826, 0838). Needs sudo.
test-netuse:
	WINE=$(PREFIX)/bin/wine test/netuse-gate.sh

# Map Network Drive asks for a name and password when a share refuses
# (0839). Needs sudo.
test-mapcred:
	WINE=$(PREFIX)/bin/wine test/mapcred-gate.sh

# AnyDesk (0860-0862): a desktop says whether it takes input (UOI_IO); a
# WinEvent hook skipping its own process skips the one that set it; the
# taskbar is not made active when the active window goes away.
test-inputdesk:
	WINE=$(PREFIX)/bin/wine test/inputdesk-gate.sh
test-winevskip:
	WINE=$(PREFIX)/bin/wine test/winevskip-gate.sh
test-noactnext:
	WINE=$(PREFIX)/bin/wine test/noactnext-gate.sh

# A pen's pressure and tilt to Windows programs, two-finger scrolling (1000,
# 1001); builds sg-compositor's test build from SG_COMPOSITOR_SRC (default:
# the sg-compositor checkout beside this one).
test-pentouch:
	WINE=$(PREFIX)/bin/wine test/pentouch-gate.sh

# Touches as touches (WM_POINTER of PT_TOUCH, WM_TOUCH, WM_GESTURE, the mouse
# for programs leaving them to DefWindowProc), edge swipes, a pen plugged in
# later and leaving range (1150); sg-compositor 0.2.0+sg40's test build from
# SG_COMPOSITOR_SRC.
test-touch:
	WINE=$(PREFIX)/bin/wine test/touch-gate.sh

# The taskbar's menu: "Show touch keyboard button" (1151).
test-keybutton:
	WINE=$(PREFIX)/bin/wine test/keybutton-gate.sh

# DirectWrite has the families FontSubstitutes name (1220): Skia's text
# (DYMO Connect's labels) in the font the screen shows.
test-dwsubst:
	WINE=$(PREFIX)/bin/wine test/dwsubst-gate.sh

# A printer's PrintCapabilities are its driver's, tickets name its papers
# (1221); its status is its CUPS queue's (1222); a queue CUPS had when a
# prefix was made is a printer of it (0924).
test-ptcaps:
	WINE=$(PREFIX)/bin/wine test/ptcaps-gate.sh

# Microsoft Edge 154 (David's Latitude 2026-10-06): a hidden, maximized window
# stays hidden on SC_MAXIMIZE (1300); the type library marshaler for bases
# with no proxy/stub of their own (1301); a class served by a service starts
# for a standard user, no needless 30-second surrogate wait (1302, needs the
# sgconf standard user); the diagnostic-data/kiosk settings classes (1303).
test-hiddenmax:
	WINE=$(PREFIX)/bin/wine test/hiddenmax-gate.sh

test-tlbbase:
	WINE=$(PREFIX)/bin/wine test/tlbbase-gate.sh

test-comsvc:
	WINE=$(PREFIX)/bin/wine bash test/comsvc-gate.sh

test-privacysettings:
	WINE=$(PREFIX)/bin/wine test/privacysettings-gate.sh

test-dwspacing:
	WINE=$(PREFIX)/bin/wine test/dwspacing-gate.sh

test-msgboxicons:
	WINE=$(PREFIX)/bin/wine test/msgboxicons-gate.sh

# Credential Manager keeps credentials in the person's keyring (1380):
# secret-tool finds what CredWrite wrote; round trips, migration, fallbacks.
test-credkeyring:
	WINE=$(PREFIX)/bin/wine test/credkeyring-gate.sh

# A pen that comes after a program started, and goes again (1420):
# WM_POINTERDEVICECHANGE, Wintab finding a late tablet, a pen gone leaving its
# window; sg-compositor 0.2.0+sg46's test tablet ("plug", "unplug") from
# SG_COMPOSITOR_SRC.
test-latepen:
	WINE=$(PREFIX)/bin/wine test/latepen-gate.sh

test-zipfolder:
	WINE=$(PREFIX)/bin/wine test/zipfolder-gate.sh

test-thumbbar:
	WINE=$(PREFIX)/bin/wine test/thumbbar-gate.sh

test-xplacement:
	WINE=$(PREFIX)/bin/wine test/xplacement-gate.sh

test-tsfsinks:
	WINE=$(PREFIX)/bin/wine test/tsfsinks-gate.sh

test-certresync:
	WINE=$(PREFIX)/bin/wine test/certresync-gate.sh

test-inputpane:
	WINE=$(PREFIX)/bin/wine test/inputpane-gate.sh

test-jumplist:
	WINE=$(PREFIX)/bin/wine test/jumplist-gate.sh

test-evtquery:
	WINE=$(PREFIX)/bin/wine test/evtquery-gate.sh

test-audiotopo:
	WINE=$(PREFIX)/bin/wine test/audiotopo-gate.sh

test-dxcore:
	WINE=$(PREFIX)/bin/wine test/dxcore-gate.sh

test-dmanip:
	WINE=$(PREFIX)/bin/wine test/dmanip-gate.sh

test-imglist2:
	WINE=$(PREFIX)/bin/wine test/imglist2-gate.sh

test-lvtv:
	WINE=$(PREFIX)/bin/wine test/lvtv-gate.sh

test-toolbar2:
	WINE=$(PREFIX)/bin/wine test/toolbar2-gate.sh

test-ccmisc:
	WINE=$(PREFIX)/bin/wine test/ccmisc-gate.sh

test-ole32misc:
	WINE=$(PREFIX)/bin/wine test/ole32misc-gate.sh

test-oapict:
	WINE=$(PREFIX)/bin/wine test/oapict-gate.sh

test-tlbcreate:
	WINE=$(PREFIX)/bin/wine test/tlbcreate-gate.sh

test-roerror:
	WINE=$(PREFIX)/bin/wine test/roerror-gate.sh

test-guires:
	WINE=$(PREFIX)/bin/wine test/guires-gate.sh

test-powerreq:
	WINE=$(PREFIX)/bin/wine test/powerreq-gate.sh

test-ncryptksp:
	WINE=$(PREFIX)/bin/wine test/ncryptksp-gate.sh

test-cryptrest:
	WINE=$(PREFIX)/bin/wine test/cryptrest-gate.sh

test-etwtrace:
	WINE=$(PREFIX)/bin/wine test/etwtrace-gate.sh

test-uiaevents:
	WINE=$(PREFIX)/bin/wine test/uiaevents-gate.sh

test-msgfilter:
	WINE=$(PREFIX)/bin/wine test/msgfilter-gate.sh

test-uiabridge:
	WINE=$(PREFIX)/bin/wine test/uiabridge-gate.sh

test-shellmisc:
	WINE=$(PREFIX)/bin/wine test/shellmisc-gate.sh

test-oleaccstd:
	WINE=$(PREFIX)/bin/wine test/oleaccstd-gate.sh

test-sockmisc:
	WINE=$(PREFIX)/bin/wine test/sockmisc-gate.sh

test-netlist:
	WINE=$(PREFIX)/bin/wine test/netlist-gate.sh

test-bestroute:
	WINE=$(PREFIX)/bin/wine test/bestroute-gate.sh

test-fileflags:
	WINE=$(PREFIX)/bin/wine test/fileflags-gate.sh

test-dwritemisc:
	WINE=$(PREFIX)/bin/wine test/dwritemisc-gate.sh

test-certfind:
	WINE=$(PREFIX)/bin/wine test/certfind-gate.sh

test-pfxexport:
	WINE=$(PREFIX)/bin/wine test/pfxexport-gate.sh

test-smallstubs3:
	WINE=$(PREFIX)/bin/wine test/smallstubs3-gate.sh

test-aptshutdown:
	WINE=$(PREFIX)/bin/wine test/aptshutdown-gate.sh

test-cmifaces:
	WINE=$(PREFIX)/bin/wine test/cmifaces-gate.sh

test-cmnotify:
	WINE=$(PREFIX)/bin/wine test/cmnotify-gate.sh

test-powerschemes:
	WINE=$(PREFIX)/bin/wine test/powerschemes-gate.sh

test-capsdhcp:
	WINE=$(PREFIX)/bin/wine test/capsdhcp-gate.sh

test-procquota:
	WINE=$(PREFIX)/bin/wine test/procquota-gate.sh

test-credprotect:
	WINE=$(PREFIX)/bin/wine test/credprotect-gate.sh

test-creduiwin:
	WINE=$(PREFIX)/bin/wine test/creduiwin-gate.sh

test-wtsnotify:
	WINE=$(PREFIX)/bin/wine test/wtsnotify-gate.sh

test-appdefault:
	WINE=$(PREFIX)/bin/wine test/appdefault-gate.sh

test-propkeyname:
	WINE=$(PREFIX)/bin/wine test/propkeyname-gate.sh

test-setupcm:
	WINE=$(PREFIX)/bin/wine test/setupcm-gate.sh

test-urlmonparse:
	WINE=$(PREFIX)/bin/wine test/urlmonparse-gate.sh

test-urlmonzone:
	WINE=$(PREFIX)/bin/wine test/urlmonzone-gate.sh

test-boundary:
	WINE=$(PREFIX)/bin/wine test/boundary-gate.sh

test-devicemap:
	WINE=$(PREFIX)/bin/wine test/devicemap-gate.sh

test-gentable:
	WINE=$(PREFIX)/bin/wine test/gentable-gate.sh

test-perflib:
	WINE=$(PREFIX)/bin/wine test/perflib-gate.sh

test-ncryptwrap:
	WINE=$(PREFIX)/bin/wine test/ncryptwrap-gate.sh

test-netuserinfo:
	WINE=$(PREFIX)/bin/wine test/netuserinfo-gate.sh

test-wintrusthelp:
	WINE=$(PREFIX)/bin/wine test/wintrusthelp-gate.sh

test-wtssession:
	WINE=$(PREFIX)/bin/wine test/wtssession-gate.sh

test-accdisp:
	WINE=$(PREFIX)/bin/wine test/accdisp-gate.sh

test-accprop:
	WINE=$(PREFIX)/bin/wine test/accprop-gate.sh

test-d3d11featx:
	WINE=$(PREFIX)/bin/wine test/d3d11featx-gate.sh

test-d3d11misc:
	WINE=$(PREFIX)/bin/wine test/d3d11misc-gate.sh

test-comctlstub:
	WINE=$(PREFIX)/bin/wine test/comctlstub-gate.sh

test-comdlgitem:
	WINE=$(PREFIX)/bin/wine test/comdlgitem-gate.sh

test-msxmlstub:
	WINE=$(PREFIX)/bin/wine test/msxmlstub-gate.sh

test-shlwapistub:
	WINE=$(PREFIX)/bin/wine test/shlwapistub-gate.sh

test-blockreason:
	WINE=$(PREFIX)/bin/wine test/blockreason-gate.sh

test-fwenv:
	WINE=$(PREFIX)/bin/wine test/fwenv-gate.sh

test-regmulti:
	WINE=$(PREFIX)/bin/wine test/regmulti-gate.sh

test-shellhook:
	WINE=$(PREFIX)/bin/wine test/shellhook-gate.sh

test-bcryptconfig:
	WINE=$(PREFIX)/bin/wine test/bcryptconfig-gate.sh

test-bcryptoaep:
	WINE=$(PREFIX)/bin/wine test/bcryptoaep-gate.sh

test-bcryptresolve:
	WINE=$(PREFIX)/bin/wine test/bcryptresolve-gate.sh

test-crypt32aux:
	WINE=$(PREFIX)/bin/wine test/crypt32aux-gate.sh

test-dnsapihelp:
	WINE=$(PREFIX)/bin/wine test/dnsapihelp-gate.sh

test-iphlpapient:
	WINE=$(PREFIX)/bin/wine test/iphlpapient-gate.sh

test-powerlegacy:
	WINE=$(PREFIX)/bin/wine test/powerlegacy-gate.sh

test-tapiline:
	WINE=$(PREFIX)/bin/wine test/tapiline-gate.sh

test-dxgistat:
	WINE=$(PREFIX)/bin/wine test/dxgistat-gate.sh

test-dxgiswap:
	WINE=$(PREFIX)/bin/wine test/dxgiswap-gate.sh

test-jpegexif:
	WINE=$(PREFIX)/bin/wine test/jpegexif-gate.sh

test-wicxform:
	WINE=$(PREFIX)/bin/wine test/wicxform-gate.sh

test-dsound-stubs:
	WINE=$(PREFIX)/bin/wine test/dsound-stubs-gate.sh

test-lockserver:
	WINE=$(PREFIX)/bin/wine test/lockserver-gate.sh

test-mf-misc:
	WINE=$(PREFIX)/bin/wine test/mf-misc-gate.sh

test-mfplat-buffers:
	WINE=$(PREFIX)/bin/wine test/mfplat-buffers-gate.sh

test-mfplat-bytestream:
	WINE=$(PREFIX)/bin/wine test/mfplat-bytestream-gate.sh

test-wgi-device:
	WINE=$(PREFIX)/bin/wine test/wgi-device-gate.sh

test-wgi-stubs:
	WINE=$(PREFIX)/bin/wine test/wgi-stubs-gate.sh

test-wgst-mpegaudio:
	WINE=$(PREFIX)/bin/wine test/wgst-mpegaudio-gate.sh

test-wgst-wmreader:
	WINE=$(PREFIX)/bin/wine test/wgst-wmreader-gate.sh

test-winmm-misc:
	WINE=$(PREFIX)/bin/wine test/winmm-misc-gate.sh

test-wmc-stubs:
	WINE=$(PREFIX)/bin/wine test/wmc-stubs-gate.sh

test-wmdev-stubs:
	WINE=$(PREFIX)/bin/wine test/wmdev-stubs-gate.sh

test-wmedia-stubs:
	WINE=$(PREFIX)/bin/wine test/wmedia-stubs-gate.sh

test-wmspeech:
	WINE=$(PREFIX)/bin/wine test/wmspeech-gate.sh

test-wmspeech2:
	WINE=$(PREFIX)/bin/wine test/wmspeech2-gate.sh

test-xinput-audio:
	WINE=$(PREFIX)/bin/wine test/xinput-audio-gate.sh

test-mitigate:
	WINE=$(PREFIX)/bin/wine test/mitigate-gate.sh

test-usersessions:
	WINE=$(PREFIX)/bin/wine test/usersessions-gate.sh

test-adocmd:
	WINE=$(PREFIX)/bin/wine test/adocmd-gate.sh

test-dtdcode:
	WINE=$(PREFIX)/bin/wine test/dtdcode-gate.sh

test-ebrowser:
	WINE=$(PREFIX)/bin/wine test/ebrowser-gate.sh

test-folderview:
	WINE=$(PREFIX)/bin/wine test/folderview-gate.sh

test-jsonweb:
	WINE=$(PREFIX)/bin/wine test/jsonweb-gate.sh

test-mimebody:
	WINE=$(PREFIX)/bin/wine test/mimebody-gate.sh

test-mimeopt:
	WINE=$(PREFIX)/bin/wine test/mimeopt-gate.sh

test-mimesave:
	WINE=$(PREFIX)/bin/wine test/mimesave-gate.sh

test-msxmlcoll:
	WINE=$(PREFIX)/bin/wine test/msxmlcoll-gate.sh

test-msxmlpersist:
	WINE=$(PREFIX)/bin/wine test/msxmlpersist-gate.sh

test-mxwdom:
	WINE=$(PREFIX)/bin/wine test/mxwdom-gate.sh

test-ofnull:
	WINE=$(PREFIX)/bin/wine test/ofnull-gate.sh

test-printfldr:
	WINE=$(PREFIX)/bin/wine test/printfldr-gate.sh

test-propbag:
	WINE=$(PREFIX)/bin/wine test/propbag-gate.sh

test-propvar2:
	WINE=$(PREFIX)/bin/wine test/propvar2-gate.sh

test-propvec:
	WINE=$(PREFIX)/bin/wine test/propvec-gate.sh

test-pvstring:
	WINE=$(PREFIX)/bin/wine test/pvstring-gate.sh

test-richedoc:
	WINE=$(PREFIX)/bin/wine test/richedoc-gate.sh

test-richeedit:
	WINE=$(PREFIX)/bin/wine test/richeedit-gate.sh

test-richepara:
	WINE=$(PREFIX)/bin/wine test/richepara-gate.sh

test-richesearch:
	WINE=$(PREFIX)/bin/wine test/richesearch-gate.sh

test-richesel:
	WINE=$(PREFIX)/bin/wine test/richesel-gate.sh

test-secidom:
	WINE=$(PREFIX)/bin/wine test/secidom-gate.sh

test-setupclass:
	WINE=$(PREFIX)/bin/wine test/setupclass-gate.sh

test-setupnodeprop:
	WINE=$(PREFIX)/bin/wine test/setupnodeprop-gate.sh

test-setupspace:
	WINE=$(PREFIX)/bin/wine test/setupspace-gate.sh

test-sfileinfo:
	WINE=$(PREFIX)/bin/wine test/sfileinfo-gate.sh

test-sfparse:
	WINE=$(PREFIX)/bin/wine test/sfparse-gate.sh

test-shexecargs:
	WINE=$(PREFIX)/bin/wine test/shexecargs-gate.sh

test-shlinkstub:
	WINE=$(PREFIX)/bin/wine test/shlinkstub-gate.sh

test-tabmru:
	WINE=$(PREFIX)/bin/wine test/tabmru-gate.sh

test-tbsep:
	WINE=$(PREFIX)/bin/wine test/tbsep-gate.sh

test-webverbs:
	WINE=$(PREFIX)/bin/wine test/webverbs-gate.sh

test-createfile:
	WINE=$(PREFIX)/bin/wine test/createfile-gate.sh

test-fcache:
	WINE=$(PREFIX)/bin/wine test/fcache-gate.sh

test-fileioctl:
	WINE=$(PREFIX)/bin/wine test/fileioctl-gate.sh

test-fsmisc:
	WINE=$(PREFIX)/bin/wine test/fsmisc-gate.sh

test-globalmem:
	WINE=$(PREFIX)/bin/wine test/globalmem-gate.sh

test-inputapi:
	WINE=$(PREFIX)/bin/wine test/inputapi-gate.sh

test-iosb:
	WINE=$(PREFIX)/bin/wine test/iosb-gate.sh

test-lznt1:
	WINE=$(PREFIX)/bin/wine test/lznt1-gate.sh

test-monhandle:
	WINE=$(PREFIX)/bin/wine test/monhandle-gate.sh

test-ntdllmisc:
	WINE=$(PREFIX)/bin/wine test/ntdllmisc-gate.sh

test-ntinfoval:
	WINE=$(PREFIX)/bin/wine test/ntinfoval-gate.sh

test-olecompat:
	WINE=$(PREFIX)/bin/wine test/olecompat-gate.sh

test-pipeid:
	WINE=$(PREFIX)/bin/wine test/pipeid-gate.sh

test-profile:
	WINE=$(PREFIX)/bin/wine test/profile-gate.sh

test-pseudowait:
	WINE=$(PREFIX)/bin/wine test/pseudowait-gate.sh

test-registry:
	WINE=$(PREFIX)/bin/wine test/registry-gate.sh

test-rpcbind:
	WINE=$(PREFIX)/bin/wine test/rpcbind-gate.sh

test-rtlquery:
	WINE=$(PREFIX)/bin/wine test/rtlquery-gate.sh

test-sppi:
	WINE=$(PREFIX)/bin/wine test/sppi-gate.sh

test-svcdup:
	WINE=$(PREFIX)/bin/wine test/svcdup-gate.sh

test-svcfail:
	WINE=$(PREFIX)/bin/wine test/svcfail-gate.sh

test-svcname:
	WINE=$(PREFIX)/bin/wine test/svcname-gate.sh

test-uimisc:
	WINE=$(PREFIX)/bin/wine test/uimisc-gate.sh

test-validdata:
	WINE=$(PREFIX)/bin/wine test/validdata-gate.sh

test-wca:
	WINE=$(PREFIX)/bin/wine test/wca-gate.sh

test-winplace:
	WINE=$(PREFIX)/bin/wine test/winplace-gate.sh

test-writevm:
	WINE=$(PREFIX)/bin/wine test/writevm-gate.sh

test-crypt32cs:
	WINE=$(PREFIX)/bin/wine test/crypt32cs-gate.sh

test-crypt32dec:
	WINE=$(PREFIX)/bin/wine test/crypt32dec-gate.sh

test-crypt32msg:
	WINE=$(PREFIX)/bin/wine test/crypt32msg-gate.sh

test-mscoree-hash:
	WINE=$(PREFIX)/bin/wine test/mscoree-hash-gate.sh

test-mscoree-sn:
	WINE=$(PREFIX)/bin/wine test/mscoree-sn-gate.sh

test-netapi32ground:
	WINE=$(PREFIX)/bin/wine test/netapi32ground-gate.sh

test-netwrite:
	WINE=$(PREFIX)/bin/wine test/netwrite-gate.sh

test-regstorethrough:
	WINE=$(PREFIX)/bin/wine test/regstorethrough-gate.sh

test-suspend:
	WINE=$(PREFIX)/bin/wine test/suspend-gate.sh

test-tapihandles:
	WINE=$(PREFIX)/bin/wine test/tapihandles-gate.sh

test-tapiphone:
	WINE=$(PREFIX)/bin/wine test/tapiphone-gate.sh

test-tapitrans:
	WINE=$(PREFIX)/bin/wine test/tapitrans-gate.sh

test-winhttpground:
	WINE=$(PREFIX)/bin/wine test/winhttpground-gate.sh

test-wininetground:
	WINE=$(PREFIX)/bin/wine test/wininetground-gate.sh

test-wintrustadmin:
	WINE=$(PREFIX)/bin/wine test/wintrustadmin-gate.sh

test-wintrustcat:
	WINE=$(PREFIX)/bin/wine test/wintrustcat-gate.sh

test-wintrustcatw:
	WINE=$(PREFIX)/bin/wine test/wintrustcatw-gate.sh

test-wintrustfind:
	WINE=$(PREFIX)/bin/wine test/wintrustfind-gate.sh

test-ws2ground:
	WINE=$(PREFIX)/bin/wine test/ws2ground-gate.sh

test-wsreader:
	WINE=$(PREFIX)/bin/wine test/wsreader-gate.sh

test-wswriter:
	WINE=$(PREFIX)/bin/wine test/wswriter-gate.sh

test-wtsaction:
	WINE=$(PREFIX)/bin/wine test/wtsaction-gate.sh

test-d2dctl:
	WINE=$(PREFIX)/bin/wine test/d2dctl-gate.sh

test-d2dmesh:
	WINE=$(PREFIX)/bin/wine test/d2dmesh-gate.sh

test-d2dtarget:
	WINE=$(PREFIX)/bin/wine test/d2dtarget-gate.sh

test-d3d113res:
	WINE=$(PREFIX)/bin/wine test/d3d113res-gate.sh

test-d3dshare:
	WINE=$(PREFIX)/bin/wine test/d3dshare-gate.sh

test-d3dxline:
	WINE=$(PREFIX)/bin/wine test/d3dxline-gate.sh

test-dwbidi:
	WINE=$(PREFIX)/bin/wine test/dwbidi-gate.sh

test-dwfaceref:
	WINE=$(PREFIX)/bin/wine test/dwfaceref-gate.sh

test-dwhit:
	WINE=$(PREFIX)/bin/wine test/dwhit-gate.sh

test-dxgidup:
	WINE=$(PREFIX)/bin/wine test/dxgidup-gate.sh

test-gdipimg:
	WINE=$(PREFIX)/bin/wine test/gdipimg-gate.sh

test-gdipstat:
	WINE=$(PREFIX)/bin/wine test/gdipstat-gate.sh

test-metawriter:
	WINE=$(PREFIX)/bin/wine test/metawriter-gate.sh

test-uiacache:
	WINE=$(PREFIX)/bin/wine test/uiacache-gate.sh

test-uiacom:
	WINE=$(PREFIX)/bin/wine test/uiacom-gate.sh

test-uiacpat:
	WINE=$(PREFIX)/bin/wine test/uiacpat-gate.sh

test-uiapat:
	WINE=$(PREFIX)/bin/wine test/uiapat-gate.sh

test-wicblock:
	WINE=$(PREFIX)/bin/wine test/wicblock-gate.sh

test-wicenc:
	WINE=$(PREFIX)/bin/wine test/wicenc-gate.sh

test-wicicc:
	WINE=$(PREFIX)/bin/wine test/wicicc-gate.sh

test-wicinfo:
	WINE=$(PREFIX)/bin/wine test/wicinfo-gate.sh

test-wicquery:
	WINE=$(PREFIX)/bin/wine test/wicquery-gate.sh

test-wicsave:
	WINE=$(PREFIX)/bin/wine test/wicsave-gate.sh

test-dinput-misc:
	WINE=$(PREFIX)/bin/wine test/dinput-misc-gate.sh


test-dmcontent-styletracks:
	WINE=$(PREFIX)/bin/wine test/dmcontent-styletracks-gate.sh

test-dmime-audiopath:
	WINE=$(PREFIX)/bin/wine test/dmime-audiopath-gate.sh

test-dmime-perf:
	WINE=$(PREFIX)/bin/wine test/dmime-perf-gate.sh

test-dmime-tracks:
	WINE=$(PREFIX)/bin/wine test/dmime-tracks-gate.sh

test-dmloader-streams:
	WINE=$(PREFIX)/bin/wine test/dmloader-streams-gate.sh


test-dplayx-dp4:
	WINE=$(PREFIX)/bin/wine test/dplayx-dp4-gate.sh

test-dplayx-lobby:
	WINE=$(PREFIX)/bin/wine test/dplayx-lobby-gate.sh

test-dplayx-sp:
	WINE=$(PREFIX)/bin/wine test/dplayx-sp-gate.sh

test-dplayx-v1:
	WINE=$(PREFIX)/bin/wine test/dplayx-v1-gate.sh

test-dpnet-session:
	WINE=$(PREFIX)/bin/wine test/dpnet-session-gate.sh

test-evr-filter:
	WINE=$(PREFIX)/bin/wine test/evr-filter-gate.sh

test-evr-mixer:
	WINE=$(PREFIX)/bin/wine test/evr-mixer-gate.sh

test-evr-presenter:
	WINE=$(PREFIX)/bin/wine test/evr-presenter-gate.sh

test-mfme:
	WINE=$(PREFIX)/bin/wine test/mfme-gate.sh

test-mfplay:
	WINE=$(PREFIX)/bin/wine test/mfplay-gate.sh

test-mfrest-helpers:
	WINE=$(PREFIX)/bin/wine test/mfrest-helpers-gate.sh

test-mfsrcsnk:
	WINE=$(PREFIX)/bin/wine test/mfsrcsnk-gate.sh

test-qdvd-nav:
	WINE=$(PREFIX)/bin/wine test/qdvd-nav-gate.sh

test-qedit-amstream:
	WINE=$(PREFIX)/bin/wine test/qedit-amstream-gate.sh

test-qedit-mediadet:
	WINE=$(PREFIX)/bin/wine test/qedit-mediadet-gate.sh

test-qedit-msvfw32:
	WINE=$(PREFIX)/bin/wine test/qedit-msvfw32-gate.sh

test-qedit-timeline:
	WINE=$(PREFIX)/bin/wine test/qedit-timeline-gate.sh

test-quartz-graph:
	WINE=$(PREFIX)/bin/wine test/quartz-graph-gate.sh

test-quartz-vmr:
	WINE=$(PREFIX)/bin/wine test/quartz-vmr-gate.sh

test-sapi-mmaudio:
	WINE=$(PREFIX)/bin/wine test/sapi-mmaudio-gate.sh

test-sapi-stream:
	WINE=$(PREFIX)/bin/wine test/sapi-stream-gate.sh

test-sapi-token:
	WINE=$(PREFIX)/bin/wine test/sapi-token-gate.sh

test-sapi-voice:
	WINE=$(PREFIX)/bin/wine test/sapi-voice-gate.sh

test-wgdmo-colorconv:
	WINE=$(PREFIX)/bin/wine test/wgdmo-colorconv-gate.sh

test-wgdmo-resampler:
	WINE=$(PREFIX)/bin/wine test/wgdmo-resampler-gate.sh

test-wgdmo-wma:
	WINE=$(PREFIX)/bin/wine test/wgdmo-wma-gate.sh

test-wgrest-sink:
	WINE=$(PREFIX)/bin/wine test/wgrest-sink-gate.sh

test-wgrest-source:
	WINE=$(PREFIX)/bin/wine test/wgrest-source-gate.sh

test-wgrest-wmreader:
	WINE=$(PREFIX)/bin/wine test/wgrest-wmreader-gate.sh

test-wgvid-aacdec:
	WINE=$(PREFIX)/bin/wine test/wgvid-aacdec-gate.sh

test-wgvid-h264enc:
	WINE=$(PREFIX)/bin/wine test/wgvid-h264enc-gate.sh

test-wgvid-vproc:
	WINE=$(PREFIX)/bin/wine test/wgvid-vproc-gate.sh

test-wgvid-wmvdec:
	WINE=$(PREFIX)/bin/wine test/wgvid-wmvdec-gate.sh

test-wmp-state:
	WINE=$(PREFIX)/bin/wine test/wmp-state-gate.sh

test-wmv-config:
	WINE=$(PREFIX)/bin/wine test/wmv-config-gate.sh

test-wmv-misc:
	WINE=$(PREFIX)/bin/wine test/wmv-misc-gate.sh

test-wmv-reader:
	WINE=$(PREFIX)/bin/wine test/wmv-reader-gate.sh

test-winstasd:
	WINE=$(PREFIX)/bin/wine test/winstasd-gate.sh

test-wtstoken:
	WINE=$(PREFIX)/bin/wine test/wtstoken-gate.sh

test-logonuser:
	WINE=$(PREFIX)/bin/wine test/logonuser-gate.sh

test-runasuser:
	WINE=$(PREFIX)/bin/wine test/runasuser-gate.sh

test-accesscheck:
	WINE=$(PREFIX)/bin/wine test/accesscheck-gate.sh

test-actctxfast:
	WINE=$(PREFIX)/bin/wine test/actctxfast-gate.sh

test-combasemisc:
	WINE=$(PREFIX)/bin/wine test/combasemisc-gate.sh

test-dirchange:
	WINE=$(PREFIX)/bin/wine test/dirchange-gate.sh

test-endtask:
	WINE=$(PREFIX)/bin/wine test/endtask-gate.sh

test-enumsettings:
	WINE=$(PREFIX)/bin/wine test/enumsettings-gate.sh

test-procinfo2:
	WINE=$(PREFIX)/bin/wine test/procinfo2-gate.sh

test-sdmisc:
	WINE=$(PREFIX)/bin/wine test/sdmisc-gate.sh

test-sysclasses:
	WINE=$(PREFIX)/bin/wine test/sysclasses-gate.sh

test-tfcreate:
	WINE=$(PREFIX)/bin/wine test/tfcreate-gate.sh

test-d2dtxt:
	WINE=$(PREFIX)/bin/wine test/d2dtxt-gate.sh

test-d2dvb:
	WINE=$(PREFIX)/bin/wine test/d2dvb-gate.sh

test-d3drmface:
	WINE=$(PREFIX)/bin/wine test/d3drmface-gate.sh

test-d3drmframe:
	WINE=$(PREFIX)/bin/wine test/d3drmframe-gate.sh

test-d3drmpath:
	WINE=$(PREFIX)/bin/wine test/d3drmpath-gate.sh

test-d3drmtexdev:
	WINE=$(PREFIX)/bin/wine test/d3drmtexdev-gate.sh

test-d3dxanim:
	WINE=$(PREFIX)/bin/wine test/d3dxanim-gate.sh

test-d3dxconv:
	WINE=$(PREFIX)/bin/wine test/d3dxconv-gate.sh

test-d3dxddspal:
	WINE=$(PREFIX)/bin/wine test/d3dxddspal-gate.sh

test-d3dxdxt:
	WINE=$(PREFIX)/bin/wine test/d3dxdxt-gate.sh

test-d3dxfont:
	WINE=$(PREFIX)/bin/wine test/d3dxfont-gate.sh

test-d3dxisect:
	WINE=$(PREFIX)/bin/wine test/d3dxisect-gate.sh

test-d3dxmesh:
	WINE=$(PREFIX)/bin/wine test/d3dxmesh-gate.sh

test-d3dxsavetex:
	WINE=$(PREFIX)/bin/wine test/d3dxsavetex-gate.sh

test-d3dxtga:
	WINE=$(PREFIX)/bin/wine test/d3dxtga-gate.sh

test-dcompobj:
	WINE=$(PREFIX)/bin/wine test/dcompobj-gate.sh

test-dwfactref:
	WINE=$(PREFIX)/bin/wine test/dwfactref-gate.sh

test-dwfont:
	WINE=$(PREFIX)/bin/wine test/dwfont-gate.sh

test-dwset:
	WINE=$(PREFIX)/bin/wine test/dwset-gate.sh

test-dwsetaxes:
	WINE=$(PREFIX)/bin/wine test/dwsetaxes-gate.sh

test-dwtab:
	WINE=$(PREFIX)/bin/wine test/dwtab-gate.sh

test-dwwrap:
	WINE=$(PREFIX)/bin/wine test/dwwrap-gate.sh

test-gdipdisp:
	WINE=$(PREFIX)/bin/wine test/gdipdisp-gate.sh

test-gdipdrv:
	WINE=$(PREFIX)/bin/wine test/gdipdrv-gate.sh

test-gdipinit:
	WINE=$(PREFIX)/bin/wine test/gdipinit-gate.sh

test-gdipmeas:
	WINE=$(PREFIX)/bin/wine test/gdipmeas-gate.sh

test-gdipnn:
	WINE=$(PREFIX)/bin/wine test/gdipnn-gate.sh

test-gdiprgn:
	WINE=$(PREFIX)/bin/wine test/gdiprgn-gate.sh

test-oleaccproxy:
	WINE=$(PREFIX)/bin/wine test/oleaccproxy-gate.sh

test-uiaiface:
	WINE=$(PREFIX)/bin/wine test/uiaiface-gate.sh

test-wicdds:
	WINE=$(PREFIX)/bin/wine test/wicdds-gate.sh

test-wichbm:
	WINE=$(PREFIX)/bin/wine test/wichbm-gate.sh

test-wicpnginfo:
	WINE=$(PREFIX)/bin/wine test/wicpnginfo-gate.sh

test-dinputtodo:
	WINE=$(PREFIX)/bin/wine test/dinputtodo-gate.sh

test-dmcontent-band:
	WINE=$(PREFIX)/bin/wine test/dmcontent-band-gate.sh

test-dmcontent-compos:
	WINE=$(PREFIX)/bin/wine test/dmcontent-compos-gate.sh

test-dmcontent-style:
	WINE=$(PREFIX)/bin/wine test/dmcontent-style-gate.sh

test-dmusic-ports:
	WINE=$(PREFIX)/bin/wine test/dmusic-ports-gate.sh

test-dmusic-synth:
	WINE=$(PREFIX)/bin/wine test/dmusic-synth-gate.sh

test-mfrest2:
	WINE=$(PREFIX)/bin/wine test/mfrest2-gate.sh

test-quartz-rest:
	WINE=$(PREFIX)/bin/wine test/quartz-rest-gate.sh

test-wmvw-header:
	WINE=$(PREFIX)/bin/wine test/wmvw-header-gate.sh

test-wmvw-profile:
	WINE=$(PREFIX)/bin/wine test/wmvw-profile-gate.sh

test-wmvw-writer:
	WINE=$(PREFIX)/bin/wine test/wmvw-writer-gate.sh

test-wmvw-xml:
	WINE=$(PREFIX)/bin/wine test/wmvw-xml-gate.sh

test-xact-wave:
	WINE=$(PREFIX)/bin/wine test/xact-wave-gate.sh

test-adoconn:
	WINE=$(PREFIX)/bin/wine test/adoconn-gate.sh

test-adorecords:
	WINE=$(PREFIX)/bin/wine test/adorecords-gate.sh

test-cwmo:
	WINE=$(PREFIX)/bin/wine test/cwmo-gate.sh

test-dinput-hidusage:
	WINE=$(PREFIX)/bin/wine test/dinput-hidusage-gate.sh

test-dinputleft:
	WINE=$(PREFIX)/bin/wine test/dinputleft-gate.sh

test-dmcompose:
	WINE=$(PREFIX)/bin/wine test/dmcompose-gate.sh

test-dmcontent-play:
	WINE=$(PREFIX)/bin/wine test/dmcontent-play-gate.sh

test-dmime-conv:
	WINE=$(PREFIX)/bin/wine test/dmime-conv-gate.sh

test-dmscript:
	WINE=$(PREFIX)/bin/wine test/dmscript-gate.sh

test-evr-classfactory:
	WINE=$(PREFIX)/bin/wine test/evr-classfactory-gate.sh

test-evr-mfexports:
	WINE=$(PREFIX)/bin/wine test/evr-mfexports-gate.sh

test-mc-audiostate:
	WINE=$(PREFIX)/bin/wine test/mc-audiostate-gate.sh

test-mf-sequencer:
	WINE=$(PREFIX)/bin/wine test/mf-sequencer-gate.sh

test-mfplat-mediabuf:
	WINE=$(PREFIX)/bin/wine test/mfplat-mediabuf-gate.sh

test-mfplat-mtconv:
	WINE=$(PREFIX)/bin/wine test/mfplat-mtconv-gate.sh

test-mfplat-pdser:
	WINE=$(PREFIX)/bin/wine test/mfplat-pdser-gate.sh

test-mfplat-plugin:
	WINE=$(PREFIX)/bin/wine test/mfplat-plugin-gate.sh

test-mfplat-vidrep:
	WINE=$(PREFIX)/bin/wine test/mfplat-vidrep-gate.sh

test-mfplat-vih:
	WINE=$(PREFIX)/bin/wine test/mfplat-vih-gate.sh

test-quartz-collections:
	WINE=$(PREFIX)/bin/wine test/quartz-collections-gate.sh

test-quartz-fullscreen:
	WINE=$(PREFIX)/bin/wine test/quartz-fullscreen-gate.sh

test-quartz-misc:
	WINE=$(PREFIX)/bin/wine test/quartz-misc-gate.sh

test-wgasync:
	WINE=$(PREFIX)/bin/wine test/wgasync-gate.sh

test-winmmthru:
	WINE=$(PREFIX)/bin/wine test/winmmthru-gate.sh

test-winmmtodo-dsound:
	WINE=$(PREFIX)/bin/wine test/winmmtodo-dsound-gate.sh

test-winmmtodo-winmm:
	WINE=$(PREFIX)/bin/wine test/winmmtodo-winmm-gate.sh

test-wmp-collections:
	WINE=$(PREFIX)/bin/wine test/wmp-collections-gate.sh

test-wmvw-sysprof:
	WINE=$(PREFIX)/bin/wine test/wmvw-sysprof-gate.sh

test-adostream:
	WINE=$(PREFIX)/bin/wine test/adostream-gate.sh

test-cc3:
	WINE=$(PREFIX)/bin/wine test/cc3-gate.sh

test-diskspace2:
	WINE=$(PREFIX)/bin/wine test/diskspace2-gate.sh

test-mimemsg:
	WINE=$(PREFIX)/bin/wine test/mimemsg-gate.sh

test-mimeprops:
	WINE=$(PREFIX)/bin/wine test/mimeprops-gate.sh

test-propconv:
	WINE=$(PREFIX)/bin/wine test/propconv-gate.sh

test-propstore2:
	WINE=$(PREFIX)/bin/wine test/propstore2-gate.sh

test-recentdocs:
	WINE=$(PREFIX)/bin/wine test/recentdocs-gate.sh

test-richpaste:
	WINE=$(PREFIX)/bin/wine test/richpaste-gate.sh

test-setupprop:
	WINE=$(PREFIX)/bin/wine test/setupprop-gate.sh

test-shcore2:
	WINE=$(PREFIX)/bin/wine test/shcore2-gate.sh

test-shdataobj:
	WINE=$(PREFIX)/bin/wine test/shdataobj-gate.sh

test-shellord:
	WINE=$(PREFIX)/bin/wine test/shellord-gate.sh

test-idealproc:
	WINE=$(PREFIX)/bin/wine test/idealproc-gate.sh

test-monikeredge:
	WINE=$(PREFIX)/bin/wine test/monikeredge-gate.sh

test-regorder:
	WINE=$(PREFIX)/bin/wine test/regorder-gate.sh

test-volumetime:
	WINE=$(PREFIX)/bin/wine test/volumetime-gate.sh

test-bcryptground:
	WINE=$(PREFIX)/bin/wine test/bcryptground-gate.sh

test-crypt32ground:
	WINE=$(PREFIX)/bin/wine test/crypt32ground-gate.sh

test-dnsapiqueryex:
	WINE=$(PREFIX)/bin/wine test/dnsapiqueryex-gate.sh

test-msicomppath:
	WINE=$(PREFIX)/bin/wine test/msicomppath-gate.sh

test-msicost:
	WINE=$(PREFIX)/bin/wine test/msicost-gate.sh

test-msifilever:
	WINE=$(PREFIX)/bin/wine test/msifilever-gate.sh

test-msiground:
	WINE=$(PREFIX)/bin/wine test/msiground-gate.sh

test-msipatch:
	WINE=$(PREFIX)/bin/wine test/msipatch-gate.sh

test-msiprodinfo:
	WINE=$(PREFIX)/bin/wine test/msiprodinfo-gate.sh

test-msisignature:
	WINE=$(PREFIX)/bin/wine test/msisignature-gate.sh

test-ncryptdpapi:
	WINE=$(PREFIX)/bin/wine test/ncryptdpapi-gate.sh

test-secur32sasl:
	WINE=$(PREFIX)/bin/wine test/secur32sasl-gate.sh

test-secur32schannel:
	WINE=$(PREFIX)/bin/wine test/secur32schannel-gate.sh

test-taskschdground:
	WINE=$(PREFIX)/bin/wine test/taskschdground-gate.sh

test-wbemcom:
	WINE=$(PREFIX)/bin/wine test/wbemcom-gate.sh

test-wbemproc:
	WINE=$(PREFIX)/bin/wine test/wbemproc-gate.sh

test-wbemsettings:
	WINE=$(PREFIX)/bin/wine test/wbemsettings-gate.sh

test-winhttpsocket:
	WINE=$(PREFIX)/bin/wine test/winhttpsocket-gate.sh

test-wininetcrack:
	WINE=$(PREFIX)/bin/wine test/wininetcrack-gate.sh

test-wininetnoreply:
	WINE=$(PREFIX)/bin/wine test/wininetnoreply-gate.sh

test-wintrusthashonly:
	WINE=$(PREFIX)/bin/wine test/wintrusthashonly-gate.sh

test-wmic:
	WINE=$(PREFIX)/bin/wine test/wmic-gate.sh

test-wsarray:
	WINE=$(PREFIX)/bin/wine test/wsarray-gate.sh

test-wsheap:
	WINE=$(PREFIX)/bin/wine test/wsheap-gate.sh

test-wslistener:
	WINE=$(PREFIX)/bin/wine test/wslistener-gate.sh

test-wsstart:
	WINE=$(PREFIX)/bin/wine test/wsstart-gate.sh

test-wsvalue:
	WINE=$(PREFIX)/bin/wine test/wsvalue-gate.sh

test-d2dbmpcc:
	WINE=$(PREFIX)/bin/wine test/d2dbmpcc-gate.sh

test-d3drmclone:
	WINE=$(PREFIX)/bin/wine test/d3drmclone-gate.sh

test-d3drmfaces:
	WINE=$(PREFIX)/bin/wine test/d3drmfaces-gate.sh

test-d3drmmb:
	WINE=$(PREFIX)/bin/wine test/d3drmmb-gate.sh

test-d3dxsampler:
	WINE=$(PREFIX)/bin/wine test/d3dxsampler-gate.sh

test-dcomp3d:
	WINE=$(PREFIX)/bin/wine test/dcomp3d-gate.sh

test-dcompsurface:
	WINE=$(PREFIX)/bin/wine test/dcompsurface-gate.sh

test-dwritewoff:
	WINE=$(PREFIX)/bin/wine test/dwritewoff-gate.sh

test-gdipconv:
	WINE=$(PREFIX)/bin/wine test/gdipconv-gate.sh

test-gdipdual:
	WINE=$(PREFIX)/bin/wine test/gdipdual-gate.sh

test-uiacachetree:
	WINE=$(PREFIX)/bin/wine test/uiacachetree-gate.sh

test-uiagroup:
	WINE=$(PREFIX)/bin/wine test/uiagroup-gate.sh

test-uianorm:
	WINE=$(PREFIX)/bin/wine test/uianorm-gate.sh

test-uiaopts:
	WINE=$(PREFIX)/bin/wine test/uiaopts-gate.sh

test-wicico:
	WINE=$(PREFIX)/bin/wine test/wicico-gate.sh

test-wicidxpal:
	WINE=$(PREFIX)/bin/wine test/wicidxpal-gate.sh

test-hklmsd:
	WINE=$(PREFIX)/bin/wine test/hklmsd-gate.sh

# A standard user's per-user ("just for me") MSI in the shared prefix: its UserData and its
# Apps & features entry (5944, 5945; Python 3.13 from the SG Store).
test-msiperuser:
	WINE=$(PREFIX)/bin/wine test/msiperuser-gate.sh

# The session's X11 hints for Linux programs (GDK_BACKEND, SDL_VIDEODRIVER, ...) stay Unix-side:
# a Windows GTK or SDL program does not see them, a Linux one started from Wine does (5946).
test-toolkitenv:
	WINE=$(PREFIX)/bin/wine test/toolkitenv-gate.sh

test-amstream-alloc:
	WINE=$(PREFIX)/bin/wine test/amstream-alloc-gate.sh

test-amstream-smoke:
	WINE=$(PREFIX)/bin/wine test/amstream-smoke-gate.sh

test-avifil32-palchange:
	WINE=$(PREFIX)/bin/wine test/avifil32-palchange-gate.sh

test-avifil32-smoke:
	WINE=$(PREFIX)/bin/wine test/avifil32-smoke-gate.sh

test-concrt-context:
	WINE=$(PREFIX)/bin/wine test/concrt-context-gate.sh

test-concrt-exc-locks:
	WINE=$(PREFIX)/bin/wine test/concrt-exc-locks-gate.sh

test-concrt-helpers:
	WINE=$(PREFIX)/bin/wine test/concrt-helpers-gate.sh

test-concrt-queue-iter:
	WINE=$(PREFIX)/bin/wine test/concrt-queue-iter-gate.sh

test-crt-cgets:
	WINE=$(PREFIX)/bin/wine test/crt-cgets-gate.sh

test-crt-concurrency:
	WINE=$(PREFIX)/bin/wine test/crt-concurrency-gate.sh

test-crt-context-yield:
	WINE=$(PREFIX)/bin/wine test/crt-context-yield-gate.sh

test-crt-dbgheap:
	WINE=$(PREFIX)/bin/wine test/crt-dbgheap-gate.sh

test-crt-eh-helpers:
	WINE=$(PREFIX)/bin/wine test/crt-eh-helpers-gate.sh

test-crt-exc-misc:
	WINE=$(PREFIX)/bin/wine test/crt-exc-misc-gate.sh

test-crt-find32i64:
	WINE=$(PREFIX)/bin/wine test/crt-find32i64-gate.sh

test-crt-heap:
	WINE=$(PREFIX)/bin/wine test/crt-heap-gate.sh

test-crt-imagebase:
	WINE=$(PREFIX)/bin/wine test/crt-imagebase-gate.sh

test-crt-internals:
	WINE=$(PREFIX)/bin/wine test/crt-internals-gate.sh

test-crt-ismbb:
	WINE=$(PREFIX)/bin/wine test/crt-ismbb-gate.sh

test-crt-iswcsym:
	WINE=$(PREFIX)/bin/wine test/crt-iswcsym-gate.sh

test-crt-mbcasemap-data:
	WINE=$(PREFIX)/bin/wine test/crt-mbcasemap-data-gate.sh

test-crt-misc:
	WINE=$(PREFIX)/bin/wine test/crt-misc-gate.sh

test-crt-norm:
	WINE=$(PREFIX)/bin/wine test/crt-norm-gate.sh

test-crt-old-handlers:
	WINE=$(PREFIX)/bin/wine test/crt-old-handlers-gate.sh

test-crt-platform:
	WINE=$(PREFIX)/bin/wine test/crt-platform-gate.sh

test-crt-portio:
	WINE=$(PREFIX)/bin/wine test/crt-portio-gate.sh

test-crt-printf:
	WINE=$(PREFIX)/bin/wine test/crt-printf-gate.sh

test-crt-scanf:
	WINE=$(PREFIX)/bin/wine test/crt-scanf-gate.sh

test-crt-string-s:
	WINE=$(PREFIX)/bin/wine test/crt-string-s-gate.sh

test-crt-systime:
	WINE=$(PREFIX)/bin/wine test/crt-systime-gate.sh

test-crt-uchar:
	WINE=$(PREFIX)/bin/wine test/crt-uchar-gate.sh

test-crt-win32-wrappers:
	WINE=$(PREFIX)/bin/wine test/crt-win32-wrappers-gate.sh

test-devenum-smoke:
	WINE=$(PREFIX)/bin/wine test/devenum-smoke-gate.sh

test-dmsynth-dls1:
	WINE=$(PREFIX)/bin/wine test/dmsynth-dls1-gate.sh

test-dpvoice-factory:
	WINE=$(PREFIX)/bin/wine test/dpvoice-factory-gate.sh

test-dsdmo-smoke:
	WINE=$(PREFIX)/bin/wine test/dsdmo-smoke-gate.sh

test-dxva2-smoke:
	WINE=$(PREFIX)/bin/wine test/dxva2-smoke-gate.sh

test-evr-fullscreen:
	WINE=$(PREFIX)/bin/wine test/evr-fullscreen-gate.sh

test-gamebar-smoke:
	WINE=$(PREFIX)/bin/wine test/gamebar-smoke-gate.sh

test-mciavi32-smoke:
	WINE=$(PREFIX)/bin/wine test/mciavi32-smoke-gate.sh

test-mciqtz32-smoke:
	WINE=$(PREFIX)/bin/wine test/mciqtz32-smoke-gate.sh

test-media-misc-smoke:
	WINE=$(PREFIX)/bin/wine test/media-misc-smoke-gate.sh

test-mediacontrol-smoke:
	WINE=$(PREFIX)/bin/wine test/mediacontrol-smoke-gate.sh

test-mf-seekpause:
	WINE=$(PREFIX)/bin/wine test/mf-seekpause-gate.sh

test-mf-smoke:
	WINE=$(PREFIX)/bin/wine test/mf-smoke-gate.sh

test-mfplat-smoke:
	WINE=$(PREFIX)/bin/wine test/mfplat-smoke-gate.sh

test-mfreadwrite-smoke:
	WINE=$(PREFIX)/bin/wine test/mfreadwrite-smoke-gate.sh

test-mmdevapi-smoke:
	WINE=$(PREFIX)/bin/wine test/mmdevapi-smoke-gate.sh

test-mp3dmod-dmo:
	WINE=$(PREFIX)/bin/wine test/mp3dmod-dmo-gate.sh

test-msacm32-metrics:
	WINE=$(PREFIX)/bin/wine test/msacm32-metrics-gate.sh

test-msacm32-smoke:
	WINE=$(PREFIX)/bin/wine test/msacm32-smoke-gate.sh

test-msdmo-smoke:
	WINE=$(PREFIX)/bin/wine test/msdmo-smoke-gate.sh

test-msvcp-access-forms:
	WINE=$(PREFIX)/bin/wine test/msvcp-access-forms-gate.sh

test-msvcp-atomic-wait:
	WINE=$(PREFIX)/bin/wine test/msvcp-atomic-wait-gate.sh

test-msvcp-c-collate:
	WINE=$(PREFIX)/bin/wine test/msvcp-c-collate-gate.sh

test-msvcp-codecvt-utf:
	WINE=$(PREFIX)/bin/wine test/msvcp-codecvt-utf-gate.sh

test-msvcp-collate:
	WINE=$(PREFIX)/bin/wine test/msvcp-collate-gate.sh

test-msvcp-eptr-swap:
	WINE=$(PREFIX)/bin/wine test/msvcp-eptr-swap-gate.sh

test-msvcp-errcat:
	WINE=$(PREFIX)/bin/wine test/msvcp-errcat-gate.sh

test-msvcp-fs-misc:
	WINE=$(PREFIX)/bin/wine test/msvcp-fs-misc-gate.sh

test-msvcp-locale-ops:
	WINE=$(PREFIX)/bin/wine test/msvcp-locale-ops-gate.sh

test-msvcp-locinfo:
	WINE=$(PREFIX)/bin/wine test/msvcp-locinfo-gate.sh

test-msvcp-makeloc:
	WINE=$(PREFIX)/bin/wine test/msvcp-makeloc-gate.sh

test-msvcp-messages:
	WINE=$(PREFIX)/bin/wine test/msvcp-messages-gate.sh

test-msvcp-misc-older:
	WINE=$(PREFIX)/bin/wine test/msvcp-misc-older-gate.sh

test-msvcp-money:
	WINE=$(PREFIX)/bin/wine test/msvcp-money-gate.sh

test-msvcp-old-exports:
	WINE=$(PREFIX)/bin/wine test/msvcp-old-exports-gate.sh

test-msvcp-old-forms:
	WINE=$(PREFIX)/bin/wine test/msvcp-old-forms-gate.sh

test-msvcp-old-members:
	WINE=$(PREFIX)/bin/wine test/msvcp-old-members-gate.sh

test-msvcp-platform:
	WINE=$(PREFIX)/bin/wine test/msvcp-platform-gate.sh

test-msvcp-random-mp:
	WINE=$(PREFIX)/bin/wine test/msvcp-random-mp-gate.sh

test-msvcp-stream-move:
	WINE=$(PREFIX)/bin/wine test/msvcp-stream-move-gate.sh

test-msvcp-streambuf:
	WINE=$(PREFIX)/bin/wine test/msvcp-streambuf-gate.sh

test-msvcp-threads-c:
	WINE=$(PREFIX)/bin/wine test/msvcp-threads-c-gate.sh

test-msvcp-timeget:
	WINE=$(PREFIX)/bin/wine test/msvcp-timeget-gate.sh

test-msvcp-ushort-read:
	WINE=$(PREFIX)/bin/wine test/msvcp-ushort-read-gate.sh

test-msvcp-ushort-read-older:
	WINE=$(PREFIX)/bin/wine test/msvcp-ushort-read-older-gate.sh

test-msvcp-wcomplex:
	WINE=$(PREFIX)/bin/wine test/msvcp-wcomplex-gate.sh

test-msvcp-wstod:
	WINE=$(PREFIX)/bin/wine test/msvcp-wstod-gate.sh

test-msvcp-xexc:
	WINE=$(PREFIX)/bin/wine test/msvcp-xexc-gate.sh

test-msvcp-xfsopen:
	WINE=$(PREFIX)/bin/wine test/msvcp-xfsopen-gate.sh

test-msvcp-yarn-facetbase:
	WINE=$(PREFIX)/bin/wine test/msvcp-yarn-facetbase-gate.sh

test-msvcp100-misc:
	WINE=$(PREFIX)/bin/wine test/msvcp100-misc-gate.sh

test-msvcp140-2-smf:
	WINE=$(PREFIX)/bin/wine test/msvcp140-2-smf-gate.sh

test-msvcp140-crt-wrappers:
	WINE=$(PREFIX)/bin/wine test/msvcp140-crt-wrappers-gate.sh

test-msvcp60-assign:
	WINE=$(PREFIX)/bin/wine test/msvcp60-assign-gate.sh

test-msvcp60-complex:
	WINE=$(PREFIX)/bin/wine test/msvcp60-complex-gate.sh

test-msvcp60-dateorder:
	WINE=$(PREFIX)/bin/wine test/msvcp60-dateorder-gate.sh

test-msvcp60-doraise:
	WINE=$(PREFIX)/bin/wine test/msvcp60-doraise-gate.sh

test-msvcp60-exceptions:
	WINE=$(PREFIX)/bin/wine test/msvcp60-exceptions-gate.sh

test-msvcp60-getcat:
	WINE=$(PREFIX)/bin/wine test/msvcp60-getcat-gate.sh

test-msvcp60-locale-call:
	WINE=$(PREFIX)/bin/wine test/msvcp60-locale-call-gate.sh

test-msvcp60-std-exceptions:
	WINE=$(PREFIX)/bin/wine test/msvcp60-std-exceptions-gate.sh

test-msvcp60-streambuf-misc:
	WINE=$(PREFIX)/bin/wine test/msvcp60-streambuf-misc-gate.sh

test-msvcp60-timeget:
	WINE=$(PREFIX)/bin/wine test/msvcp60-timeget-gate.sh

test-msvcp60-traits-assign:
	WINE=$(PREFIX)/bin/wine test/msvcp60-traits-assign-gate.sh

test-msvcp70-complex:
	WINE=$(PREFIX)/bin/wine test/msvcp70-complex-gate.sh

test-msvcr100-task-canceled:
	WINE=$(PREFIX)/bin/wine test/msvcr100-task-canceled-gate.sh

test-msvfw32-smoke:
	WINE=$(PREFIX)/bin/wine test/msvfw32-smoke-gate.sh

test-qasf-smoke:
	WINE=$(PREFIX)/bin/wine test/qasf-smoke-gate.sh

test-qcap-smoke:
	WINE=$(PREFIX)/bin/wine test/qcap-smoke-gate.sh

test-qdvd-smoke:
	WINE=$(PREFIX)/bin/wine test/qdvd-smoke-gate.sh

test-quartz-dsound-focus:
	WINE=$(PREFIX)/bin/wine test/quartz-dsound-focus-gate.sh

test-quartz-graph-forward:
	WINE=$(PREFIX)/bin/wine test/quartz-graph-forward-gate.sh

test-quartz-vmr9-alloc:
	WINE=$(PREFIX)/bin/wine test/quartz-vmr9-alloc-gate.sh

test-rtworkq-platform-events:
	WINE=$(PREFIX)/bin/wine test/rtworkq-platform-events-gate.sh

test-ucrt-dmath:
	WINE=$(PREFIX)/bin/wine test/ucrt-dmath-gate.sh

test-vcomp-dynamic-i8:
	WINE=$(PREFIX)/bin/wine test/vcomp-dynamic-i8-gate.sh

test-vcrt-module-wrappers:
	WINE=$(PREFIX)/bin/wine test/vcrt-module-wrappers-gate.sh

test-wmp-moniker:
	WINE=$(PREFIX)/bin/wine test/wmp-moniker-gate.sh

test-wmvcore-playlist-burn:
	WINE=$(PREFIX)/bin/wine test/wmvcore-playlist-burn-gate.sh

test-wmvcore-smoke:
	WINE=$(PREFIX)/bin/wine test/wmvcore-smoke-gate.sh

test-advapi32-efs:
	WINE=$(PREFIX)/bin/wine test/advapi32-efs-gate.sh

test-advapi32-hwprofile:
	WINE=$(PREFIX)/bin/wine test/advapi32-hwprofile-gate.sh

test-advapi32-reflection:
	WINE=$(PREFIX)/bin/wine test/advapi32-reflection-gate.sh

test-advapi32-servicebits:
	WINE=$(PREFIX)/bin/wine test/advapi32-servicebits-gate.sh

test-bcrypt-aeswrap:
	WINE=$(PREFIX)/bin/wine test/bcrypt-aeswrap-gate.sh

test-bcrypt-algnotfound:
	WINE=$(PREFIX)/bin/wine test/bcrypt-algnotfound-gate.sh

test-bcrypt-decryptflags:
	WINE=$(PREFIX)/bin/wine test/bcrypt-decryptflags-gate.sh

test-bcrypt-importpair:
	WINE=$(PREFIX)/bin/wine test/bcrypt-importpair-gate.sh

test-bcrypt-propnotsupported:
	WINE=$(PREFIX)/bin/wine test/bcrypt-propnotsupported-gate.sh

test-combase-ctxcallback:
	WINE=$(PREFIX)/bin/wine test/combase-ctxcallback-gate.sh

test-combase-ctxprops:
	WINE=$(PREFIX)/bin/wine test/combase-ctxprops-gate.sh

test-combase-defctx:
	WINE=$(PREFIX)/bin/wine test/combase-defctx-gate.sh

test-combase-handlerconnected:
	WINE=$(PREFIX)/bin/wine test/combase-handlerconnected-gate.sh

test-combase-hpalette:
	WINE=$(PREFIX)/bin/wine test/combase-hpalette-gate.sh

test-combase-logicalid:
	WINE=$(PREFIX)/bin/wine test/combase-logicalid-gate.sh

test-combase-stdmarshal-disconnect:
	WINE=$(PREFIX)/bin/wine test/combase-stdmarshal-disconnect-gate.sh

test-combase-stdmarshal-null:
	WINE=$(PREFIX)/bin/wine test/combase-stdmarshal-null-gate.sh

test-crypt32-adddisposition:
	WINE=$(PREFIX)/bin/wine test/crypt32-adddisposition-gate.sh

test-crypt32-closecheck:
	WINE=$(PREFIX)/bin/wine test/crypt32-closecheck-gate.sh

test-crypt32-hexa:
	WINE=$(PREFIX)/bin/wine test/crypt32-hexa-gate.sh

test-crypt32-universalstring:
	WINE=$(PREFIX)/bin/wine test/crypt32-universalstring-gate.sh

test-cryptobuffer-convert:
	WINE=$(PREFIX)/bin/wine test/cryptobuffer-convert-gate.sh

test-cryptowinrt-inspectable:
	WINE=$(PREFIX)/bin/wine test/cryptowinrt-inspectable-gate.sh

test-gdi32-smalltextout:
	WINE=$(PREFIX)/bin/wine test/gdi32-smalltextout-gate.sh

test-kernel32-pipesession:
	WINE=$(PREFIX)/bin/wine test/kernel32-pipesession-gate.sh

test-kernel32-pipestate:
	WINE=$(PREFIX)/bin/wine test/kernel32-pipestate-gate.sh

test-kernel32-regquota:
	WINE=$(PREFIX)/bin/wine test/kernel32-regquota-gate.sh

test-kernel32-symlinka:
	WINE=$(PREFIX)/bin/wine test/kernel32-symlinka-gate.sh

test-kernel32-ums:
	WINE=$(PREFIX)/bin/wine test/kernel32-ums-gate.sh

test-kernel32-wow64selector:
	WINE=$(PREFIX)/bin/wine test/kernel32-wow64selector-gate.sh

test-kernel32-zombifyactctx:
	WINE=$(PREFIX)/bin/wine test/kernel32-zombifyactctx-gate.sh

test-kernelbase-affinityupdate:
	WINE=$(PREFIX)/bin/wine test/kernelbase-affinityupdate-gate.sh

test-kernelbase-consolemouse:
	WINE=$(PREFIX)/bin/wine test/kernelbase-consolemouse-gate.sh

test-kernelbase-georfc1766:
	WINE=$(PREFIX)/bin/wine test/kernelbase-georfc1766-gate.sh

test-kernelbase-shregcreate:
	WINE=$(PREFIX)/bin/wine test/kernelbase-shregcreate-gate.sh

test-kernelbase-shregdel:
	WINE=$(PREFIX)/bin/wine test/kernelbase-shregdel-gate.sh

test-kernelbase-urlescapecomponent:
	WINE=$(PREFIX)/bin/wine test/kernelbase-urlescapecomponent-gate.sh

test-kernelbase-urlfixup:
	WINE=$(PREFIX)/bin/wine test/kernelbase-urlfixup-gate.sh

test-kernelbase-urlisappliable:
	WINE=$(PREFIX)/bin/wine test/kernelbase-urlisappliable-gate.sh

test-ntdll-bitmap-backward:
	WINE=$(PREFIX)/bin/wine test/ntdll-bitmap-backward-gate.sh

test-ntdll-compactheap:
	WINE=$(PREFIX)/bin/wine test/ntdll-compactheap-gate.sh

test-ntdll-compressioninfo:
	WINE=$(PREFIX)/bin/wine test/ntdll-compressioninfo-gate.sh

test-ntdll-filealign:
	WINE=$(PREFIX)/bin/wine test/ntdll-filealign-gate.sh

test-ntdll-flushwrites:
	WINE=$(PREFIX)/bin/wine test/ntdll-flushwrites-gate.sh

test-ntdll-fsobjectid:
	WINE=$(PREFIX)/bin/wine test/ntdll-fsobjectid-gate.sh

test-ntdll-iopending:
	WINE=$(PREFIX)/bin/wine test/ntdll-iopending-gate.sh

test-ntdll-placeholder:
	WINE=$(PREFIX)/bin/wine test/ntdll-placeholder-gate.sh

test-ntdll-powercaps:
	WINE=$(PREFIX)/bin/wine test/ntdll-powercaps-gate.sh

test-ntdll-querydir-async:
	WINE=$(PREFIX)/bin/wine test/ntdll-querydir-async-gate.sh

test-ntdll-regalign64:
	WINE=$(PREFIX)/bin/wine test/ntdll-regalign64-gate.sh

test-ntdll-regioninfo:
	WINE=$(PREFIX)/bin/wine test/ntdll-regioninfo-gate.sh

test-ntdll-subsystem:
	WINE=$(PREFIX)/bin/wine test/ntdll-subsystem-gate.sh

test-ntdll-symlink:
	WINE=$(PREFIX)/bin/wine test/ntdll-symlink-gate.sh

test-ntdll-timerqueue-event:
	WINE=$(PREFIX)/bin/wine test/ntdll-timerqueue-event-gate.sh

test-ntdll-xpress-edges:
	WINE=$(PREFIX)/bin/wine test/ntdll-xpress-edges-gate.sh

test-ntdll-xpress:
	WINE=$(PREFIX)/bin/wine test/ntdll-xpress-gate.sh

test-ntoskrnl-callback:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-callback-gate.sh

test-ntoskrnl-confinfo:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-confinfo-gate.sh

test-ntoskrnl-csq:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-csq-gate.sh

test-ntoskrnl-dpc:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-dpc-gate.sh

test-ntoskrnl-dpcqueue:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-dpcqueue-gate.sh

test-ntoskrnl-drvext:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-drvext-gate.sh

test-ntoskrnl-errorlog:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-errorlog-gate.sh

test-ntoskrnl-harderror:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-harderror-gate.sh

test-ntoskrnl-iocreate:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-iocreate-gate.sh

test-ntoskrnl-iotimer:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-iotimer-gate.sh

test-ntoskrnl-loadimage:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-loadimage-gate.sh

test-ntoskrnl-mdl:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-mdl-gate.sh

test-ntoskrnl-netiotest:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-netiotest-gate.sh

test-ntoskrnl-ntddi:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-ntddi-gate.sh

test-ntoskrnl-obref:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-obref-gate.sh

test-ntoskrnl-pnptest:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-pnptest-gate.sh

test-ntoskrnl-procgroup:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-procgroup-gate.sh

test-ntoskrnl-procname:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-procname-gate.sh

test-ntoskrnl-queue:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-queue-gate.sh

test-ntoskrnl-regs:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-regs-gate.sh

test-ntoskrnl-reinit:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-reinit-gate.sh

test-ntoskrnl-resclaim:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-resclaim-gate.sh

test-ntoskrnl-shutdown:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-shutdown-gate.sh

test-ntoskrnl-startio:
	WINE=$(PREFIX)/bin/wine test/ntoskrnl-startio-gate.sh

test-ole32-cachefreeze:
	WINE=$(PREFIX)/bin/wine test/ole32-cachefreeze-gate.sh

test-ole32-callas:
	WINE=$(PREFIX)/bin/wine test/ole32-callas-gate.sh

test-ole32-colorset:
	WINE=$(PREFIX)/bin/wine test/ole32-colorset-gate.sh

test-ole32-enumskip:
	WINE=$(PREFIX)/bin/wine test/ole32-enumskip-gate.sh

test-ole32-handler:
	WINE=$(PREFIX)/bin/wine test/ole32-handler-gate.sh

test-ole32-monikers:
	WINE=$(PREFIX)/bin/wine test/ole32-monikers-gate.sh

test-ole32-olestreamex:
	WINE=$(PREFIX)/bin/wine test/ole32-olestreamex-gate.sh

test-ole32-proptimes:
	WINE=$(PREFIX)/bin/wine test/ole32-proptimes-gate.sh

test-ole32-setdata:
	WINE=$(PREFIX)/bin/wine test/ole32-setdata-gate.sh

test-ole32-staticdata:
	WINE=$(PREFIX)/bin/wine test/ole32-staticdata-gate.sh

test-ole32-stgconvert:
	WINE=$(PREFIX)/bin/wine test/ole32-stgconvert-gate.sh

test-ole32-stgopenex:
	WINE=$(PREFIX)/bin/wine test/ole32-stgopenex-gate.sh

test-ole32-viewcallas:
	WINE=$(PREFIX)/bin/wine test/ole32-viewcallas-gate.sh

test-oleaut32-hfont:
	WINE=$(PREFIX)/bin/wine test/oleaut32-hfont-gate.sh

test-oleaut32-propbag-qa:
	WINE=$(PREFIX)/bin/wine test/oleaut32-propbag-qa-gate.sh

test-oleaut32-propframe-initial:
	WINE=$(PREFIX)/bin/wine test/oleaut32-propframe-initial-gate.sh

test-oleaut32-recinfo:
	WINE=$(PREFIX)/bin/wine test/oleaut32-recinfo-gate.sh

test-oleaut32-tlcallas:
	WINE=$(PREFIX)/bin/wine test/oleaut32-tlcallas-gate.sh

test-oleaut32-typelibname:
	WINE=$(PREFIX)/bin/wine test/oleaut32-typelibname-gate.sh

test-onlineid-inspectable:
	WINE=$(PREFIX)/bin/wine test/onlineid-inspectable-gate.sh

test-rpcrt4-async:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-async-gate.sh

test-rpcrt4-async-rpc:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-async-rpc-gate.sh

test-rpcrt4-canceltimeout:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-canceltimeout-gate.sh

test-rpcrt4-clearout:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-clearout-gate.sh

test-rpcrt4-comtimeout:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-comtimeout-gate.sh

test-rpcrt4-convert:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-convert-gate.sh

test-rpcrt4-cstruct:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-cstruct-gate.sh

test-rpcrt4-fcwstring:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-fcwstring-gate.sh

test-rpcrt4-httprts:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-httprts-gate.sh

test-rpcrt4-ifids:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-ifids-gate.sh

test-rpcrt4-pipe:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-pipe-gate.sh

test-rpcrt4-ptrgap:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-ptrgap-gate.sh

test-rpcrt4-unregifex:
	WINE=$(PREFIX)/bin/wine test/rpcrt4-unregifex-gate.sh

test-user32-ddedisconnect:
	WINE=$(PREFIX)/bin/wine test/user32-ddedisconnect-gate.sh

test-webcore-inspectable:
	WINE=$(PREFIX)/bin/wine test/webcore-inspectable-gate.sh

test-webcore-mapview:
	WINE=$(PREFIX)/bin/wine test/webcore-mapview-gate.sh

test-win32u-coloradjust:
	WINE=$(PREFIX)/bin/wine test/win32u-coloradjust-gate.sh

test-win32u-getpixelclip:
	WINE=$(PREFIX)/bin/wine test/win32u-getpixelclip-gate.sh

test-win32u-lockwindowupdate:
	WINE=$(PREFIX)/bin/wine test/win32u-lockwindowupdate-gate.sh

test-win32u-mouseinpointer:
	WINE=$(PREFIX)/bin/wine test/win32u-mouseinpointer-gate.sh

test-win32u-openadapterhdc:
	WINE=$(PREFIX)/bin/wine test/win32u-openadapterhdc-gate.sh

test-win32u-openadaptername:
	WINE=$(PREFIX)/bin/wine test/win32u-openadaptername-gate.sh

test-gdi32-aspect:
	WINE=$(PREFIX)/bin/wine test/gdi32-aspect-gate.sh

test-gdi32-colorspace:
	WINE=$(PREFIX)/bin/wine test/gdi32-colorspace-gate.sh

test-gdi32-emf-escapes:
	WINE=$(PREFIX)/bin/wine test/gdi32-emf-escapes-gate.sh

test-gdi32-engclip:
	WINE=$(PREFIX)/bin/wine test/gdi32-engclip-gate.sh

test-gdi32-gcpclass:
	WINE=$(PREFIX)/bin/wine test/gdi32-gcpclass-gate.sh

test-gdi32-opentype:
	WINE=$(PREFIX)/bin/wine test/gdi32-opentype-gate.sh

test-gdi32-updateicmregkey:
	WINE=$(PREFIX)/bin/wine test/gdi32-updateicmregkey-gate.sh

test-ntdll-sppi:
	WINE=$(PREFIX)/bin/wine test/ntdll-sppi-gate.sh

test-user32-animrects:
	WINE=$(PREFIX)/bin/wine test/user32-animrects-gate.sh

test-user32-dataobj:
	WINE=$(PREFIX)/bin/wine test/user32-dataobj-gate.sh

test-user32-ddelist:
	WINE=$(PREFIX)/bin/wine test/user32-ddelist-gate.sh

test-user32-ddepartner:
	WINE=$(PREFIX)/bin/wine test/user32-ddepartner-gate.sh

test-user32-devnotify-ansi:
	WINE=$(PREFIX)/bin/wine test/user32-devnotify-ansi-gate.sh

test-user32-dpihosting:
	WINE=$(PREFIX)/bin/wine test/user32-dpihosting-gate.sh

test-user32-msgbox-help:
	WINE=$(PREFIX)/bin/wine test/user32-msgbox-help-gate.sh

test-user32-setdisplayconfig:
	WINE=$(PREFIX)/bin/wine test/user32-setdisplayconfig-gate.sh

test-user32-touchht:
	WINE=$(PREFIX)/bin/wine test/user32-touchht-gate.sh

test-user32-winnls-ime:
	WINE=$(PREFIX)/bin/wine test/user32-winnls-ime-gate.sh

test-usp10-justify:
	WINE=$(PREFIX)/bin/wine test/usp10-justify-gate.sh

test-win32u-ddescape:
	WINE=$(PREFIX)/bin/wine test/win32u-ddescape-gate.sh

test-win32u-eto-numerics:
	WINE=$(PREFIX)/bin/wine test/win32u-eto-numerics-gate.sh

test-win32u-flash-timer:
	WINE=$(PREFIX)/bin/wine test/win32u-flash-timer-gate.sh

test-win32u-hkl-alias:
	WINE=$(PREFIX)/bin/wine test/win32u-hkl-alias-gate.sh

test-win32u-memfont:
	WINE=$(PREFIX)/bin/wine test/win32u-memfont-gate.sh

test-win32u-menu-autodismiss:
	WINE=$(PREFIX)/bin/wine test/win32u-menu-autodismiss-gate.sh

test-win32u-qai:
	WINE=$(PREFIX)/bin/wine test/win32u-qai-gate.sh

test-win32u-spi-access:
	WINE=$(PREFIX)/bin/wine test/win32u-spi-access-gate.sh

test-win32u-wgl-sharelists:
	WINE=$(PREFIX)/bin/wine test/win32u-wgl-sharelists-gate.sh

test-richrc:
	WINE=$(PREFIX)/bin/wine test/richrc-gate.sh

test-widl-pipe:
	WINE=$(PREFIX)/bin/wine test/widl-pipe-gate.sh

test-fastactiveds:
	WINE=$(PREFIX)/bin/wine test/fastactiveds-gate.sh

test-fastaddmon:
	WINE=$(PREFIX)/bin/wine test/fastaddmon-gate.sh

test-fastado:
	WINE=$(PREFIX)/bin/wine test/fastado-gate.sh

test-fastado2:
	WINE=$(PREFIX)/bin/wine test/fastado2-gate.sh

test-fastadsldp:
	WINE=$(PREFIX)/bin/wine test/fastadsldp-gate.sh

test-fastadvpack:
	WINE=$(PREFIX)/bin/wine test/fastadvpack-gate.sh

test-fastagile:
	WINE=$(PREFIX)/bin/wine test/fastagile-gate.sh

test-fastappdata:
	WINE=$(PREFIX)/bin/wine test/fastappdata-gate.sh

test-fastappmodel:
	WINE=$(PREFIX)/bin/wine test/fastappmodel-gate.sh

test-fastatl-ax:
	WINE=$(PREFIX)/bin/wine test/fastatl-ax-gate.sh

test-fastatl-ax2:
	WINE=$(PREFIX)/bin/wine test/fastatl-ax2-gate.sh

test-fastatl-misc:
	WINE=$(PREFIX)/bin/wine test/fastatl-misc-gate.sh

test-fastatl30:
	WINE=$(PREFIX)/bin/wine test/fastatl30-gate.sh

test-fastbrowseui:
	WINE=$(PREFIX)/bin/wine test/fastbrowseui-gate.sh

test-fastcab:
	WINE=$(PREFIX)/bin/wine test/fastcab-gate.sh

test-fastcdlg:
	WINE=$(PREFIX)/bin/wine test/fastcdlg-gate.sh

test-fastcloudasn1:
	WINE=$(PREFIX)/bin/wine test/fastcloudasn1-gate.sh

test-fastcloudkbd:
	WINE=$(PREFIX)/bin/wine test/fastcloudkbd-gate.sh

test-fastcloudmapi:
	WINE=$(PREFIX)/bin/wine test/fastcloudmapi-gate.sh

test-fastcloudnewdev:
	WINE=$(PREFIX)/bin/wine test/fastcloudnewdev-gate.sh

test-fastcloudpcap:
	WINE=$(PREFIX)/bin/wine test/fastcloudpcap-gate.sh

test-fastcloudprntvpt:
	WINE=$(PREFIX)/bin/wine test/fastcloudprntvpt-gate.sh

test-fastcloudqos:
	WINE=$(PREFIX)/bin/wine test/fastcloudqos-gate.sh

test-fastcloudspool:
	WINE=$(PREFIX)/bin/wine test/fastcloudspool-gate.sh

test-fastcloudsppc:
	WINE=$(PREFIX)/bin/wine test/fastcloudsppc-gate.sh

test-fastcloudtbs:
	WINE=$(PREFIX)/bin/wine test/fastcloudtbs-gate.sh

test-fastcloudurl:
	WINE=$(PREFIX)/bin/wine test/fastcloudurl-gate.sh

test-fastcloudwsh:
	WINE=$(PREFIX)/bin/wine test/fastcloudwsh-gate.sh

test-fastclusapi:
	WINE=$(PREFIX)/bin/wine test/fastclusapi-gate.sh

test-fastcomsvcs:
	WINE=$(PREFIX)/bin/wine test/fastcomsvcs-gate.sh

test-fastcoremsg:
	WINE=$(PREFIX)/bin/wine test/fastcoremsg-gate.sh

test-fastdataexchange:
	WINE=$(PREFIX)/bin/wine test/fastdataexchange-gate.sh

test-fastdbgeng:
	WINE=$(PREFIX)/bin/wine test/fastdbgeng-gate.sh

test-fastddraw:
	WINE=$(PREFIX)/bin/wine test/fastddraw-gate.sh

test-fastdevenum:
	WINE=$(PREFIX)/bin/wine test/fastdevenum-gate.sh

test-fastdhtmled-b2:
	WINE=$(PREFIX)/bin/wine test/fastdhtmled-b2-gate.sh

test-fastdhtmled:
	WINE=$(PREFIX)/bin/wine test/fastdhtmled-gate.sh

test-fastdsui:
	WINE=$(PREFIX)/bin/wine test/fastdsui-gate.sh

test-fastdwjust:
	WINE=$(PREFIX)/bin/wine test/fastdwjust-gate.sh

test-fastdx8vb:
	WINE=$(PREFIX)/bin/wine test/fastdx8vb-gate.sh

test-fastdxgirc:
	WINE=$(PREFIX)/bin/wine test/fastdxgirc-gate.sh

test-fastesent:
	WINE=$(PREFIX)/bin/wine test/fastesent-gate.sh

test-fastesent2:
	WINE=$(PREFIX)/bin/wine test/fastesent2-gate.sh

test-fastextrac32:
	WINE=$(PREFIX)/bin/wine test/fastextrac32-gate.sh

test-fastfileio:
	WINE=$(PREFIX)/bin/wine test/fastfileio-gate.sh

test-fastfusion:
	WINE=$(PREFIX)/bin/wine test/fastfusion-gate.sh

test-fastgameux:
	WINE=$(PREFIX)/bin/wine test/fastgameux-gate.sh

test-fastglobalization:
	WINE=$(PREFIX)/bin/wine test/fastglobalization-gate.sh

test-fasthhctrl:
	WINE=$(PREFIX)/bin/wine test/fasthhctrl-gate.sh

test-fasthidtranslate:
	WINE=$(PREFIX)/bin/wine test/fasthidtranslate-gate.sh

test-fasthnetcfg:
	WINE=$(PREFIX)/bin/wine test/fasthnetcfg-gate.sh

test-fasthnetcfg2:
	WINE=$(PREFIX)/bin/wine test/fasthnetcfg2-gate.sh

test-fasthttpapi:
	WINE=$(PREFIX)/bin/wine test/fasthttpapi-gate.sh

test-fastie-bs:
	WINE=$(PREFIX)/bin/wine test/fastie-bs-gate.sh

test-fastie:
	WINE=$(PREFIX)/bin/wine test/fastie-gate.sh

test-fastie-mshtml:
	WINE=$(PREFIX)/bin/wine test/fastie-mshtml-gate.sh

test-fastie-riched:
	WINE=$(PREFIX)/bin/wine test/fastie-riched-gate.sh

test-fastie-urlmon:
	WINE=$(PREFIX)/bin/wine test/fastie-urlmon-gate.sh

test-fastieframe-b1:
	WINE=$(PREFIX)/bin/wine test/fastieframe-b1-gate.sh

test-fastieframe-events:
	WINE=$(PREFIX)/bin/wine test/fastieframe-events-gate.sh

test-fastieframe:
	WINE=$(PREFIX)/bin/wine test/fastieframe-gate.sh

test-fastieframe-host:
	WINE=$(PREFIX)/bin/wine test/fastieframe-host-gate.sh

test-fastieframe-move:
	WINE=$(PREFIX)/bin/wine test/fastieframe-move-gate.sh

test-fastieframe-newwin:
	WINE=$(PREFIX)/bin/wine test/fastieframe-newwin-gate.sh

test-fastieframe-showui:
	WINE=$(PREFIX)/bin/wine test/fastieframe-showui-gate.sh

test-fastieshortcut:
	WINE=$(PREFIX)/bin/wine test/fastieshortcut-gate.sh

test-fastimm-ime:
	WINE=$(PREFIX)/bin/wine test/fastimm-ime-gate.sh

test-fastinetcomm:
	WINE=$(PREFIX)/bin/wine test/fastinetcomm-gate.sh

test-fastinetedit:
	WINE=$(PREFIX)/bin/wine test/fastinetedit-gate.sh

test-fastinetintl:
	WINE=$(PREFIX)/bin/wine test/fastinetintl-gate.sh

test-fastinseng:
	WINE=$(PREFIX)/bin/wine test/fastinseng-gate.sh

test-fastinspect:
	WINE=$(PREFIX)/bin/wine test/fastinspect-gate.sh

test-fastiphlp:
	WINE=$(PREFIX)/bin/wine test/fastiphlp-gate.sh

test-fastiq:
	WINE=$(PREFIX)/bin/wine test/fastiq-gate.sh

test-fastitss:
	WINE=$(PREFIX)/bin/wine test/fastitss-gate.sh

test-fastjsb1:
	WINE=$(PREFIX)/bin/wine test/fastjsb1-gate.sh

test-fastjscall:
	WINE=$(PREFIX)/bin/wine test/fastjscall-gate.sh

test-fastjserr:
	WINE=$(PREFIX)/bin/wine test/fastjserr-gate.sh

test-fastjsintl:
	WINE=$(PREFIX)/bin/wine test/fastjsintl-gate.sh

test-fastjsmisc:
	WINE=$(PREFIX)/bin/wine test/fastjsmisc-gate.sh

test-fastjsmode:
	WINE=$(PREFIX)/bin/wine test/fastjsmode-gate.sh

test-fastjsproxy:
	WINE=$(PREFIX)/bin/wine test/fastjsproxy-gate.sh

test-fastjstyped:
	WINE=$(PREFIX)/bin/wine test/fastjstyped-gate.sh

test-fastlongtail:
	WINE=$(PREFIX)/bin/wine test/fastlongtail-gate.sh

test-fastmapi:
	WINE=$(PREFIX)/bin/wine test/fastmapi-gate.sh

test-fastmapi2:
	WINE=$(PREFIX)/bin/wine test/fastmapi2-gate.sh

test-fastmlang:
	WINE=$(PREFIX)/bin/wine test/fastmlang-gate.sh

test-fastmpr:
	WINE=$(PREFIX)/bin/wine test/fastmpr-gate.sh

test-fastmprapi:
	WINE=$(PREFIX)/bin/wine test/fastmprapi-gate.sh

test-fastmsctf:
	WINE=$(PREFIX)/bin/wine test/fastmsctf-gate.sh

test-fastmsctfb1:
	WINE=$(PREFIX)/bin/wine test/fastmsctfb1-gate.sh

test-fastmsctfb2:
	WINE=$(PREFIX)/bin/wine test/fastmsctfb2-gate.sh

test-fastmsctfprof:
	WINE=$(PREFIX)/bin/wine test/fastmsctfprof-gate.sh

test-fastmsdaps:
	WINE=$(PREFIX)/bin/wine test/fastmsdaps-gate.sh

test-fastmsdapsrow:
	WINE=$(PREFIX)/bin/wine test/fastmsdapsrow-gate.sh

test-fastmsdasql:
	WINE=$(PREFIX)/bin/wine test/fastmsdasql-gate.sh

test-fastmshtml:
	WINE=$(PREFIX)/bin/wine test/fastmshtml-gate.sh

test-fastmshtmladopt:
	WINE=$(PREFIX)/bin/wine test/fastmshtmladopt-gate.sh

test-fastmshtmlb1:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlb1-gate.sh

test-fastmshtmlb2:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlb2-gate.sh

test-fastmshtmlb3:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlb3-gate.sh

test-fastmshtmlb4:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlb4-gate.sh

test-fastmshtmlb5:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlb5-gate.sh

test-fastmshtmlcssom:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlcssom-gate.sh

test-fastmshtmldm:
	WINE=$(PREFIX)/bin/wine test/fastmshtmldm-gate.sh

test-fastmshtmldoc7:
	WINE=$(PREFIX)/bin/wine test/fastmshtmldoc7-gate.sh

test-fastmshtmldom:
	WINE=$(PREFIX)/bin/wine test/fastmshtmldom-gate.sh

test-fastmshtmldomex:
	WINE=$(PREFIX)/bin/wine test/fastmshtmldomex-gate.sh

test-fastmshtmlerr:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlerr-gate.sh

test-fastmshtmlevent:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlevent-gate.sh

test-fastmshtmlfile:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlfile-gate.sh

test-fastmshtmlmarkup:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlmarkup-gate.sh

test-fastmshtmlmedia:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlmedia-gate.sh

test-fastmshtmlmisc:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlmisc-gate.sh

test-fastmshtmlmo:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlmo-gate.sh

test-fastmshtmlopen:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlopen-gate.sh

test-fastmshtmlpointer:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlpointer-gate.sh

test-fastmshtmlrangecmd:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlrangecmd-gate.sh

test-fastmshtmlrc:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlrc-gate.sh

test-fastmshtmlsel:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlsel-gate.sh

test-fastmshtmltrav:
	WINE=$(PREFIX)/bin/wine test/fastmshtmltrav-gate.sh

test-fastmshtmlundo:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlundo-gate.sh

test-fastmshtmlunits:
	WINE=$(PREFIX)/bin/wine test/fastmshtmlunits-gate.sh

test-fastmsi:
	WINE=$(PREFIX)/bin/wine test/fastmsi-gate.sh

test-fastmsiaut:
	WINE=$(PREFIX)/bin/wine test/fastmsiaut-gate.sh

test-fastmsib1:
	WINE=$(PREFIX)/bin/wine test/fastmsib1-gate.sh

test-fastmsib2:
	WINE=$(PREFIX)/bin/wine test/fastmsib2-gate.sh

test-fastmsiini:
	WINE=$(PREFIX)/bin/wine test/fastmsiini-gate.sh

test-fastmsimode:
	WINE=$(PREFIX)/bin/wine test/fastmsimode-gate.sh

test-fastmsimtf:
	WINE=$(PREFIX)/bin/wine test/fastmsimtf-gate.sh

test-fastmsipkg:
	WINE=$(PREFIX)/bin/wine test/fastmsipkg-gate.sh

test-fastmsiprov:
	WINE=$(PREFIX)/bin/wine test/fastmsiprov-gate.sh

test-fastmsisrc:
	WINE=$(PREFIX)/bin/wine test/fastmsisrc-gate.sh

test-fastmsiusage:
	WINE=$(PREFIX)/bin/wine test/fastmsiusage-gate.sh

test-fastmsscript:
	WINE=$(PREFIX)/bin/wine test/fastmsscript-gate.sh

test-fastmsxml-b1:
	WINE=$(PREFIX)/bin/wine test/fastmsxml-b1-gate.sh

test-fastmsxml-b2:
	WINE=$(PREFIX)/bin/wine test/fastmsxml-b2-gate.sh

test-fastmsxml:
	WINE=$(PREFIX)/bin/wine test/fastmsxml-gate.sh

test-fastmsxml3:
	WINE=$(PREFIX)/bin/wine test/fastmsxml3-gate.sh

test-fastmsxmlmisc:
	WINE=$(PREFIX)/bin/wine test/fastmsxmlmisc-gate.sh

test-fastmsxmlsax:
	WINE=$(PREFIX)/bin/wine test/fastmsxmlsax-gate.sh

test-fastmsxmlwriter:
	WINE=$(PREFIX)/bin/wine test/fastmsxmlwriter-gate.sh

test-fastmsxmlxslt:
	WINE=$(PREFIX)/bin/wine test/fastmsxmlxslt-gate.sh

test-fastnet-emit1:
	WINE=$(PREFIX)/bin/wine test/fastnet-emit1-gate.sh

test-fastnet-host1:
	WINE=$(PREFIX)/bin/wine test/fastnet-host1-gate.sh

test-fastnet-meta1:
	WINE=$(PREFIX)/bin/wine test/fastnet-meta1-gate.sh

test-fastnet-sn1:
	WINE=$(PREFIX)/bin/wine test/fastnet-sn1-gate.sh

test-fastnet-sn2:
	WINE=$(PREFIX)/bin/wine test/fastnet-sn2-gate.sh

test-fastnetacct:
	WINE=$(PREFIX)/bin/wine test/fastnetacct-gate.sh

test-fastnetapi:
	WINE=$(PREFIX)/bin/wine test/fastnetapi-gate.sh

test-fastnetbios:
	WINE=$(PREFIX)/bin/wine test/fastnetbios-gate.sh

test-fastnetmisc:
	WINE=$(PREFIX)/bin/wine test/fastnetmisc-gate.sh

test-fastninput:
	WINE=$(PREFIX)/bin/wine test/fastninput-gate.sh

test-fastodbccp32:
	WINE=$(PREFIX)/bin/wine test/fastodbccp32-gate.sh

test-fastoldgfx:
	WINE=$(PREFIX)/bin/wine test/fastoldgfx-gate.sh

test-fastole32rc:
	WINE=$(PREFIX)/bin/wine test/fastole32rc-gate.sh

test-fastoledb32:
	WINE=$(PREFIX)/bin/wine test/fastoledb32-gate.sh

test-fastoledlg:
	WINE=$(PREFIX)/bin/wine test/fastoledlg-gate.sh

test-fastopencl:
	WINE=$(PREFIX)/bin/wine test/fastopencl-gate.sh

test-fastpackager:
	WINE=$(PREFIX)/bin/wine test/fastpackager-gate.sh

test-fastpdh:
	WINE=$(PREFIX)/bin/wine test/fastpdh-gate.sh

test-fastpf:
	WINE=$(PREFIX)/bin/wine test/fastpf-gate.sh

test-fastprintui:
	WINE=$(PREFIX)/bin/wine test/fastprintui-gate.sh

test-fastrasapi:
	WINE=$(PREFIX)/bin/wine test/fastrasapi-gate.sh

test-fastrasbook:
	WINE=$(PREFIX)/bin/wine test/fastrasbook-gate.sh

test-fastregini:
	WINE=$(PREFIX)/bin/wine test/fastregini-gate.sh

test-fastrometa2:
	WINE=$(PREFIX)/bin/wine test/fastrometa2-gate.sh

test-fastrometadata:
	WINE=$(PREFIX)/bin/wine test/fastrometadata-gate.sh

test-fastrstrtmgr:
	WINE=$(PREFIX)/bin/wine test/fastrstrtmgr-gate.sh

test-fastscrobj:
	WINE=$(PREFIX)/bin/wine test/fastscrobj-gate.sh

test-fastsensapi:
	WINE=$(PREFIX)/bin/wine test/fastsensapi-gate.sh

test-fastservices:
	WINE=$(PREFIX)/bin/wine test/fastservices-gate.sh

test-fastsetjob:
	WINE=$(PREFIX)/bin/wine test/fastsetjob-gate.sh

test-fastsetupapi:
	WINE=$(PREFIX)/bin/wine test/fastsetupapi-gate.sh

test-fastsetupimg:
	WINE=$(PREFIX)/bin/wine test/fastsetupimg-gate.sh

test-fastshdocvw:
	WINE=$(PREFIX)/bin/wine test/fastshdocvw-gate.sh

test-fastshdocvw2:
	WINE=$(PREFIX)/bin/wine test/fastshdocvw2-gate.sh

test-fastshell32rc:
	WINE=$(PREFIX)/bin/wine test/fastshell32rc-gate.sh

test-fastshlwapi3:
	WINE=$(PREFIX)/bin/wine test/fastshlwapi3-gate.sh

test-fastshmisc:
	WINE=$(PREFIX)/bin/wine test/fastshmisc-gate.sh

test-fastsmbios:
	WINE=$(PREFIX)/bin/wine test/fastsmbios-gate.sh

test-fastsockiosb:
	WINE=$(PREFIX)/bin/wine test/fastsockiosb-gate.sh

test-fastsubst:
	WINE=$(PREFIX)/bin/wine test/fastsubst-gate.sh

test-fastsvcenum:
	WINE=$(PREFIX)/bin/wine test/fastsvcenum-gate.sh

test-fasttapi:
	WINE=$(PREFIX)/bin/wine test/fasttapi-gate.sh

test-fastthreadpool:
	WINE=$(PREFIX)/bin/wine test/fastthreadpool-gate.sh

test-fastuiarc:
	WINE=$(PREFIX)/bin/wine test/fastuiarc-gate.sh

test-fastuiarepeat:
	WINE=$(PREFIX)/bin/wine test/fastuiarepeat-gate.sh

test-fasturlmon-b1:
	WINE=$(PREFIX)/bin/wine test/fasturlmon-b1-gate.sh

test-fasturlmon-b2:
	WINE=$(PREFIX)/bin/wine test/fasturlmon-b2-gate.sh

test-fasturlmon:
	WINE=$(PREFIX)/bin/wine test/fasturlmon-gate.sh

test-fastvbb1:
	WINE=$(PREFIX)/bin/wine test/fastvbb1-gate.sh

test-fastvbenum:
	WINE=$(PREFIX)/bin/wine test/fastvbenum-gate.sh

test-fastvberr:
	WINE=$(PREFIX)/bin/wine test/fastvberr-gate.sh

test-fastvbfor:
	WINE=$(PREFIX)/bin/wine test/fastvbfor-gate.sh

test-fastvbparens:
	WINE=$(PREFIX)/bin/wine test/fastvbparens-gate.sh

test-fastvbs:
	WINE=$(PREFIX)/bin/wine test/fastvbs-gate.sh

test-fastvidcap:
	WINE=$(PREFIX)/bin/wine test/fastvidcap-gate.sh

test-fastvssapi:
	WINE=$(PREFIX)/bin/wine test/fastvssapi-gate.sh

test-fastwbemdisp:
	WINE=$(PREFIX)/bin/wine test/fastwbemdisp-gate.sh

test-fastwbemsink:
	WINE=$(PREFIX)/bin/wine test/fastwbemsink-gate.sh

test-fastwebabort:
	WINE=$(PREFIX)/bin/wine test/fastwebabort-gate.sh

test-fastwebb1:
	WINE=$(PREFIX)/bin/wine test/fastwebb1-gate.sh

test-fastwebdesc:
	WINE=$(PREFIX)/bin/wine test/fastwebdesc-gate.sh

test-fastwebhdr:
	WINE=$(PREFIX)/bin/wine test/fastwebhdr-gate.sh

test-fastwebhttp:
	WINE=$(PREFIX)/bin/wine test/fastwebhttp-gate.sh

test-fastwebserv:
	WINE=$(PREFIX)/bin/wine test/fastwebserv-gate.sh

test-fastwebservices:
	WINE=$(PREFIX)/bin/wine test/fastwebservices-gate.sh

test-fastwebsvc2:
	WINE=$(PREFIX)/bin/wine test/fastwebsvc2-gate.sh

test-fastwebtypes:
	WINE=$(PREFIX)/bin/wine test/fastwebtypes-gate.sh

test-fastwinhlp:
	WINE=$(PREFIX)/bin/wine test/fastwinhlp-gate.sh

test-fastwinhttp:
	WINE=$(PREFIX)/bin/wine test/fastwinhttp-gate.sh

test-fastwininet-b1:
	WINE=$(PREFIX)/bin/wine test/fastwininet-b1-gate.sh

test-fastwininet:
	WINE=$(PREFIX)/bin/wine test/fastwininet-gate.sh

test-fastwininetenum:
	WINE=$(PREFIX)/bin/wine test/fastwininetenum-gate.sh

test-fastwininetgroup:
	WINE=$(PREFIX)/bin/wine test/fastwininetgroup-gate.sh

test-fastwininetopt:
	WINE=$(PREFIX)/bin/wine test/fastwininetopt-gate.sh

test-fastwininetrc:
	WINE=$(PREFIX)/bin/wine test/fastwininetrc-gate.sh

test-fastwinstorage:
	WINE=$(PREFIX)/bin/wine test/fastwinstorage-gate.sh

test-fastwintypes:
	WINE=$(PREFIX)/bin/wine test/fastwintypes-gate.sh

test-fastwinui:
	WINE=$(PREFIX)/bin/wine test/fastwinui-gate.sh

test-fastwinui-sched:
	WINE=$(PREFIX)/bin/wine test/fastwinui-sched-gate.sh

test-fastwlanrc:
	WINE=$(PREFIX)/bin/wine test/fastwlanrc-gate.sh

test-fastwpc:
	WINE=$(PREFIX)/bin/wine test/fastwpc-gate.sh

test-fastwstorage2:
	WINE=$(PREFIX)/bin/wine test/fastwstorage2-gate.sh

test-fastwuapi:
	WINE=$(PREFIX)/bin/wine test/fastwuapi-gate.sh

test-fastwuapi2:
	WINE=$(PREFIX)/bin/wine test/fastwuapi2-gate.sh

test-fastwuapiset:
	WINE=$(PREFIX)/bin/wine test/fastwuapiset-gate.sh

test-fastxcfg:
	WINE=$(PREFIX)/bin/wine test/fastxcfg-gate.sh

test-fastxmllite:
	WINE=$(PREFIX)/bin/wine test/fastxmllite-gate.sh

test-fastxolehlp:
	WINE=$(PREFIX)/bin/wine test/fastxolehlp-gate.sh

test-g-shared-d3d11:
	WINE=$(PREFIX)/bin/wine test/g-shared-d3d11-gate.sh

test-g-shared-fence:
	WINE=$(PREFIX)/bin/wine test/g-shared-fence-gate.sh

test-g-shared-kmt:
	WINE=$(PREFIX)/bin/wine test/g-shared-kmt-gate.sh

test-g-shared-kmtmutex:
	WINE=$(PREFIX)/bin/wine test/g-shared-kmtmutex-gate.sh

test-g-shared-kmtsync:
	WINE=$(PREFIX)/bin/wine test/g-shared-kmtsync-gate.sh

test-msix2:
	WINE=$(PREFIX)/bin/wine test/msix2-gate.sh

test-fastb-abterm:
	WINE=$(PREFIX)/bin/wine test/fastb-abterm-gate.sh

test-fastb-acct:
	WINE=$(PREFIX)/bin/wine test/fastb-acct-gate.sh

test-fastb-activeds:
	WINE=$(PREFIX)/bin/wine test/fastb-activeds-gate.sh

test-fastb-addmon:
	WINE=$(PREFIX)/bin/wine test/fastb-addmon-gate.sh

test-fastb-adesk:
	WINE=$(PREFIX)/bin/wine test/fastb-adesk-gate.sh

test-fastb-advpack:
	WINE=$(PREFIX)/bin/wine test/fastb-advpack-gate.sh

test-fastb-amsi:
	WINE=$(PREFIX)/bin/wine test/fastb-amsi-gate.sh

test-fastb-apphelp:
	WINE=$(PREFIX)/bin/wine test/fastb-apphelp-gate.sh

test-fastb-authz:
	WINE=$(PREFIX)/bin/wine test/fastb-authz-gate.sh

test-fastb-bitsup:
	WINE=$(PREFIX)/bin/wine test/fastb-bitsup-gate.sh

test-fastb-bt:
	WINE=$(PREFIX)/bin/wine test/fastb-bt-gate.sh

test-fastb-btdev:
	WINE=$(PREFIX)/bin/wine test/fastb-btdev-gate.sh

test-fastb-bthprops:
	WINE=$(PREFIX)/bin/wine test/fastb-bthprops-gate.sh

test-fastb-cdm:
	WINE=$(PREFIX)/bin/wine test/fastb-cdm-gate.sh

test-fastb-cfgmgr:
	WINE=$(PREFIX)/bin/wine test/fastb-cfgmgr-gate.sh

test-fastb-cmeject:
	WINE=$(PREFIX)/bin/wine test/fastb-cmeject-gate.sh

test-fastb-cmenu:
	WINE=$(PREFIX)/bin/wine test/fastb-cmenu-gate.sh

test-fastb-cmenuids:
	WINE=$(PREFIX)/bin/wine test/fastb-cmenuids-gate.sh

test-fastb-comcaller:
	WINE=$(PREFIX)/bin/wine test/fastb-comcaller-gate.sh

test-fastb-comctl:
	WINE=$(PREFIX)/bin/wine test/fastb-comctl-gate.sh

test-fastb-compose:
	WINE=$(PREFIX)/bin/wine test/fastb-compose-gate.sh

test-fastb-conalias:
	WINE=$(PREFIX)/bin/wine test/fastb-conalias-gate.sh

test-fastb-cordebug:
	WINE=$(PREFIX)/bin/wine test/fastb-cordebug-gate.sh

test-fastb-corefix:
	WINE=$(PREFIX)/bin/wine test/fastb-corefix-gate.sh

test-fastb-creddom:
	WINE=$(PREFIX)/bin/wine test/fastb-creddom-gate.sh

test-fastb-cryptui:
	WINE=$(PREFIX)/bin/wine test/fastb-cryptui-gate.sh

test-fastb-d3d11vp:
	WINE=$(PREFIX)/bin/wine test/fastb-d3d11vp-gate.sh

test-fastb-d3dpatch:
	WINE=$(PREFIX)/bin/wine test/fastb-d3dpatch-gate.sh

test-fastb-d3drm:
	WINE=$(PREFIX)/bin/wine test/fastb-d3drm-gate.sh

test-fastb-d3drm2:
	WINE=$(PREFIX)/bin/wine test/fastb-d3drm2-gate.sh

test-fastb-d3drm3:
	WINE=$(PREFIX)/bin/wine test/fastb-d3drm3-gate.sh

test-fastb-d3dx:
	WINE=$(PREFIX)/bin/wine test/fastb-d3dx-gate.sh

test-fastb-d3dx10:
	WINE=$(PREFIX)/bin/wine test/fastb-d3dx10-gate.sh

test-fastb-d3dx11:
	WINE=$(PREFIX)/bin/wine test/fastb-d3dx11-gate.sh

test-fastb-d3dxfx:
	WINE=$(PREFIX)/bin/wine test/fastb-d3dxfx-gate.sh

test-fastb-d3dxload:
	WINE=$(PREFIX)/bin/wine test/fastb-d3dxload-gate.sh

test-fastb-d3dxtex:
	WINE=$(PREFIX)/bin/wine test/fastb-d3dxtex-gate.sh

test-fastb-dad:
	WINE=$(PREFIX)/bin/wine test/fastb-dad-gate.sh

test-fastb-dbgeng:
	WINE=$(PREFIX)/bin/wine test/fastb-dbgeng-gate.sh

test-fastb-dbghelp:
	WINE=$(PREFIX)/bin/wine test/fastb-dbghelp-gate.sh

test-fastb-ddewild:
	WINE=$(PREFIX)/bin/wine test/fastb-ddewild-gate.sh

test-fastb-dia:
	WINE=$(PREFIX)/bin/wine test/fastb-dia-gate.sh

test-fastb-dns:
	WINE=$(PREFIX)/bin/wine test/fastb-dns-gate.sh

test-fastb-dwmthumb:
	WINE=$(PREFIX)/bin/wine test/fastb-dwmthumb-gate.sh

test-fastb-dwrite:
	WINE=$(PREFIX)/bin/wine test/fastb-dwrite-gate.sh

test-fastb-dxgi:
	WINE=$(PREFIX)/bin/wine test/fastb-dxgi-gate.sh

test-fastb-ebstate:
	WINE=$(PREFIX)/bin/wine test/fastb-ebstate-gate.sh

test-fastb-edp:
	WINE=$(PREFIX)/bin/wine test/fastb-edp-gate.sh

test-fastb-efill:
	WINE=$(PREFIX)/bin/wine test/fastb-efill-gate.sh

test-fastb-enummui:
	WINE=$(PREFIX)/bin/wine test/fastb-enummui-gate.sh

test-fastb-fipsmode:
	WINE=$(PREFIX)/bin/wine test/fastb-fipsmode-gate.sh

test-fastb-fltlib:
	WINE=$(PREFIX)/bin/wine test/fastb-fltlib-gate.sh

test-fastb-fontkey:
	WINE=$(PREFIX)/bin/wine test/fastb-fontkey-gate.sh

test-fastb-fvdual:
	WINE=$(PREFIX)/bin/wine test/fastb-fvdual-gate.sh

test-fastb-gapbluetooth:
	WINE=$(PREFIX)/bin/wine test/fastb-gapbluetooth-gate.sh

test-fastb-gapcf:
	WINE=$(PREFIX)/bin/wine test/fastb-gapcf-gate.sh

test-fastb-gapconnectivity:
	WINE=$(PREFIX)/bin/wine test/fastb-gapconnectivity-gate.sh

test-fastb-gapd2d:
	WINE=$(PREFIX)/bin/wine test/fastb-gapd2d-gate.sh

test-fastb-gapd3dx:
	WINE=$(PREFIX)/bin/wine test/fastb-gapd3dx-gate.sh

test-fastb-gapd3dx2:
	WINE=$(PREFIX)/bin/wine test/fastb-gapd3dx2-gate.sh

test-fastb-gapdbg:
	WINE=$(PREFIX)/bin/wine test/fastb-gapdbg-gate.sh

test-fastb-gapdbgeng:
	WINE=$(PREFIX)/bin/wine test/fastb-gapdbgeng-gate.sh

test-fastb-gapdcomp:
	WINE=$(PREFIX)/bin/wine test/fastb-gapdcomp-gate.sh

test-fastb-gapddraw:
	WINE=$(PREFIX)/bin/wine test/fastb-gapddraw-gate.sh

test-fastb-gapdrm:
	WINE=$(PREFIX)/bin/wine test/fastb-gapdrm-gate.sh

test-fastb-gapexp:
	WINE=$(PREFIX)/bin/wine test/fastb-gapexp-gate.sh

test-fastb-gapfence:
	WINE=$(PREFIX)/bin/wine test/fastb-gapfence-gate.sh

test-fastb-gapgdip:
	WINE=$(PREFIX)/bin/wine test/fastb-gapgdip-gate.sh

test-fastb-gapgp:
	WINE=$(PREFIX)/bin/wine test/fastb-gapgp-gate.sh

test-fastb-gaphlink:
	WINE=$(PREFIX)/bin/wine test/fastb-gaphlink-gate.sh

test-fastb-gaphostname:
	WINE=$(PREFIX)/bin/wine test/fastb-gaphostname-gate.sh

test-fastb-gapimagehlp:
	WINE=$(PREFIX)/bin/wine test/fastb-gapimagehlp-gate.sh

test-fastb-gapimagehlp2:
	WINE=$(PREFIX)/bin/wine test/fastb-gapimagehlp2-gate.sh

test-fastb-gapinseng:
	WINE=$(PREFIX)/bin/wine test/fastb-gapinseng-gate.sh

test-fastb-gapmisc:
	WINE=$(PREFIX)/bin/wine test/fastb-gapmisc-gate.sh

test-fastb-gapmscms:
	WINE=$(PREFIX)/bin/wine test/fastb-gapmscms-gate.sh

test-fastb-gapmscoree2:
	WINE=$(PREFIX)/bin/wine test/fastb-gapmscoree2-gate.sh

test-fastb-gapmsscript:
	WINE=$(PREFIX)/bin/wine test/fastb-gapmsscript-gate.sh

test-fastb-gapmstask:
	WINE=$(PREFIX)/bin/wine test/fastb-gapmstask-gate.sh

test-fastb-gapogl:
	WINE=$(PREFIX)/bin/wine test/fastb-gapogl-gate.sh

test-fastb-gapoledb:
	WINE=$(PREFIX)/bin/wine test/fastb-gapoledb-gate.sh

test-fastb-gappropsys:
	WINE=$(PREFIX)/bin/wine test/fastb-gappropsys-gate.sh

test-fastb-gapprov:
	WINE=$(PREFIX)/bin/wine test/fastb-gapprov-gate.sh

test-fastb-gapre:
	WINE=$(PREFIX)/bin/wine test/fastb-gapre-gate.sh

test-fastb-gapsechost:
	WINE=$(PREFIX)/bin/wine test/fastb-gapsechost-gate.sh

test-fastb-gapshcore:
	WINE=$(PREFIX)/bin/wine test/fastb-gapshcore-gate.sh

test-fastb-gapshell:
	WINE=$(PREFIX)/bin/wine test/fastb-gapshell-gate.sh

test-fastb-gapspool:
	WINE=$(PREFIX)/bin/wine test/fastb-gapspool-gate.sh

test-fastb-gapsti:
	WINE=$(PREFIX)/bin/wine test/fastb-gapsti-gate.sh

test-fastb-gapuia:
	WINE=$(PREFIX)/bin/wine test/fastb-gapuia-gate.sh

test-fastb-gapwbemprox2:
	WINE=$(PREFIX)/bin/wine test/fastb-gapwbemprox2-gate.sh

test-fastb-gapwd3d:
	WINE=$(PREFIX)/bin/wine test/fastb-gapwd3d-gate.sh

test-fastb-gapwd3d9:
	WINE=$(PREFIX)/bin/wine test/fastb-gapwd3d9-gate.sh

test-fastb-gapwia:
	WINE=$(PREFIX)/bin/wine test/fastb-gapwia-gate.sh

test-fastb-gapwic:
	WINE=$(PREFIX)/bin/wine test/fastb-gapwic-gate.sh

test-fastb-gapwintab:
	WINE=$(PREFIX)/bin/wine test/fastb-gapwintab-gate.sh

test-fastb-gapwsdapi:
	WINE=$(PREFIX)/bin/wine test/fastb-gapwsdapi-gate.sh

test-fastb-gapwsock32:
	WINE=$(PREFIX)/bin/wine test/fastb-gapwsock32-gate.sh

test-fastb-gdip:
	WINE=$(PREFIX)/bin/wine test/fastb-gdip-gate.sh

test-fastb-gdipsmall:
	WINE=$(PREFIX)/bin/wine test/fastb-gdipsmall-gate.sh

test-fastb-geo:
	WINE=$(PREFIX)/bin/wine test/fastb-geo-gate.sh

test-fastb-gldef:
	WINE=$(PREFIX)/bin/wine test/fastb-gldef-gate.sh

test-fastb-hidstack:
	WINE=$(PREFIX)/bin/wine test/fastb-hidstack-gate.sh

test-fastb-hlink:
	WINE=$(PREFIX)/bin/wine test/fastb-hlink-gate.sh

test-fastb-imm:
	WINE=$(PREFIX)/bin/wine test/fastb-imm-gate.sh

test-fastb-ink:
	WINE=$(PREFIX)/bin/wine test/fastb-ink-gate.sh

test-fastb-itss:
	WINE=$(PREFIX)/bin/wine test/fastb-itss-gate.sh

test-fastb-kbdload:
	WINE=$(PREFIX)/bin/wine test/fastb-kbdload-gate.sh

test-fastb-kernel:
	WINE=$(PREFIX)/bin/wine test/fastb-kernel-gate.sh

test-fastb-kfm:
	WINE=$(PREFIX)/bin/wine test/fastb-kfm-gate.sh

test-fastb-kvol:
	WINE=$(PREFIX)/bin/wine test/fastb-kvol-gate.sh

test-fastb-logonsess:
	WINE=$(PREFIX)/bin/wine test/fastb-logonsess-gate.sh

test-fastb-mapdebug:
	WINE=$(PREFIX)/bin/wine test/fastb-mapdebug-gate.sh

test-fastb-mpr:
	WINE=$(PREFIX)/bin/wine test/fastb-mpr-gate.sh

test-fastb-mscms:
	WINE=$(PREFIX)/bin/wine test/fastb-mscms-gate.sh

test-fastb-mscoree:
	WINE=$(PREFIX)/bin/wine test/fastb-mscoree-gate.sh

test-fastb-msgdecstream:
	WINE=$(PREFIX)/bin/wine test/fastb-msgdecstream-gate.sh

test-fastb-msgstream:
	WINE=$(PREFIX)/bin/wine test/fastb-msgstream-gate.sh

test-fastb-msident:
	WINE=$(PREFIX)/bin/wine test/fastb-msident-gate.sh

test-fastb-mssign:
	WINE=$(PREFIX)/bin/wine test/fastb-mssign-gate.sh

test-fastb-mstask:
	WINE=$(PREFIX)/bin/wine test/fastb-mstask-gate.sh

test-fastb-ncrypt:
	WINE=$(PREFIX)/bin/wine test/fastb-ncrypt-gate.sh

test-fastb-netapi:
	WINE=$(PREFIX)/bin/wine test/fastb-netapi-gate.sh

test-fastb-netcfg:
	WINE=$(PREFIX)/bin/wine test/fastb-netcfg-gate.sh

test-fastb-netconn:
	WINE=$(PREFIX)/bin/wine test/fastb-netconn-gate.sh

test-fastb-ntds:
	WINE=$(PREFIX)/bin/wine test/fastb-ntds-gate.sh

test-fastb-odbc:
	WINE=$(PREFIX)/bin/wine test/fastb-odbc-gate.sh

test-fastb-ole1:
	WINE=$(PREFIX)/bin/wine test/fastb-ole1-gate.sh

test-fastb-oledb32:
	WINE=$(PREFIX)/bin/wine test/fastb-oledb32-gate.sh

test-fastb-opc:
	WINE=$(PREFIX)/bin/wine test/fastb-opc-gate.sh

test-fastb-physstore:
	WINE=$(PREFIX)/bin/wine test/fastb-physstore-gate.sh

test-fastb-propsys:
	WINE=$(PREFIX)/bin/wine test/fastb-propsys-gate.sh

test-fastb-rbin:
	WINE=$(PREFIX)/bin/wine test/fastb-rbin-gate.sh

test-fastb-rdrop:
	WINE=$(PREFIX)/bin/wine test/fastb-rdrop-gate.sh

test-fastb-retxtsrv:
	WINE=$(PREFIX)/bin/wine test/fastb-retxtsrv-gate.sh

test-fastb-ribbon:
	WINE=$(PREFIX)/bin/wine test/fastb-ribbon-gate.sh

test-fastb-rpcifids:
	WINE=$(PREFIX)/bin/wine test/fastb-rpcifids-gate.sh

test-fastb-rtmmcss:
	WINE=$(PREFIX)/bin/wine test/fastb-rtmmcss-gate.sh

test-fastb-rtwq:
	WINE=$(PREFIX)/bin/wine test/fastb-rtwq-gate.sh

test-fastb-rtwqjoin:
	WINE=$(PREFIX)/bin/wine test/fastb-rtwqjoin-gate.sh

test-fastb-rtxs:
	WINE=$(PREFIX)/bin/wine test/fastb-rtxs-gate.sh

test-fastb-sarrelease:
	WINE=$(PREFIX)/bin/wine test/fastb-sarrelease-gate.sh

test-fastb-sdimach:
	WINE=$(PREFIX)/bin/wine test/fastb-sdimach-gate.sh

test-fastb-secacl:
	WINE=$(PREFIX)/bin/wine test/fastb-secacl-gate.sh

test-fastb-sechost:
	WINE=$(PREFIX)/bin/wine test/fastb-sechost-gate.sh

test-fastb-seckb:
	WINE=$(PREFIX)/bin/wine test/fastb-seckb-gate.sh

test-fastb-sens:
	WINE=$(PREFIX)/bin/wine test/fastb-sens-gate.sh

test-fastb-setupunmapped:
	WINE=$(PREFIX)/bin/wine test/fastb-setupunmapped-gate.sh

test-fastb-sfview:
	WINE=$(PREFIX)/bin/wine test/fastb-sfview-gate.sh

test-fastb-sfvmsg:
	WINE=$(PREFIX)/bin/wine test/fastb-sfvmsg-gate.sh

test-fastb-shcore:
	WINE=$(PREFIX)/bin/wine test/fastb-shcore-gate.sh

test-fastb-shell2:
	WINE=$(PREFIX)/bin/wine test/fastb-shell2-gate.sh

test-fastb-shell3:
	WINE=$(PREFIX)/bin/wine test/fastb-shell3-gate.sh

test-fastb-shell4:
	WINE=$(PREFIX)/bin/wine test/fastb-shell4-gate.sh

test-fastb-shfileop:
	WINE=$(PREFIX)/bin/wine test/fastb-shfileop-gate.sh

test-fastb-shfileop2:
	WINE=$(PREFIX)/bin/wine test/fastb-shfileop2-gate.sh

test-fastb-shimg:
	WINE=$(PREFIX)/bin/wine test/fastb-shimg-gate.sh

test-fastb-shitem:
	WINE=$(PREFIX)/bin/wine test/fastb-shitem-gate.sh

test-fastb-shmisc:
	WINE=$(PREFIX)/bin/wine test/fastb-shmisc-gate.sh

test-fastb-sxs:
	WINE=$(PREFIX)/bin/wine test/fastb-sxs-gate.sh

test-fastb-syscur:
	WINE=$(PREFIX)/bin/wine test/fastb-syscur-gate.sh

test-fastb-sysproto:
	WINE=$(PREFIX)/bin/wine test/fastb-sysproto-gate.sh

test-fastb-taskschd:
	WINE=$(PREFIX)/bin/wine test/fastb-taskschd-gate.sh

test-fastb-tdh:
	WINE=$(PREFIX)/bin/wine test/fastb-tdh-gate.sh

test-fastb-tdhprop:
	WINE=$(PREFIX)/bin/wine test/fastb-tdhprop-gate.sh

test-fastb-thprio:
	WINE=$(PREFIX)/bin/wine test/fastb-thprio-gate.sh

test-fastb-tom:
	WINE=$(PREFIX)/bin/wine test/fastb-tom-gate.sh

test-fastb-tom2:
	WINE=$(PREFIX)/bin/wine test/fastb-tom2-gate.sh

test-fastb-tools:
	WINE=$(PREFIX)/bin/wine test/fastb-tools-gate.sh

test-fastb-ucv:
	WINE=$(PREFIX)/bin/wine test/fastb-ucv-gate.sh

test-fastb-ucvagile:
	WINE=$(PREFIX)/bin/wine test/fastb-ucvagile-gate.sh

test-fastb-uia:
	WINE=$(PREFIX)/bin/wine test/fastb-uia-gate.sh

test-fastb-uiaannot:
	WINE=$(PREFIX)/bin/wine test/fastb-uiaannot-gate.sh

test-fastb-uiacache:
	WINE=$(PREFIX)/bin/wine test/fastb-uiacache-gate.sh

test-fastb-uiafind:
	WINE=$(PREFIX)/bin/wine test/fastb-uiafind-gate.sh

test-fastb-uianc:
	WINE=$(PREFIX)/bin/wine test/fastb-uianc-gate.sh

test-fastb-uiatext:
	WINE=$(PREFIX)/bin/wine test/fastb-uiatext-gate.sh

test-fastb-uncnp:
	WINE=$(PREFIX)/bin/wine test/fastb-uncnp-gate.sh

test-fastb-userenv:
	WINE=$(PREFIX)/bin/wine test/fastb-userenv-gate.sh

test-fastb-uxanim:
	WINE=$(PREFIX)/bin/wine test/fastb-uxanim-gate.sh

test-fastb-vfold:
	WINE=$(PREFIX)/bin/wine test/fastb-vfold-gate.sh

test-fastb-wbem:
	WINE=$(PREFIX)/bin/wine test/fastb-wbem-gate.sh

test-fastb-wbemps:
	WINE=$(PREFIX)/bin/wine test/fastb-wbemps-gate.sh

test-fastb-wer:
	WINE=$(PREFIX)/bin/wine test/fastb-wer-gate.sh

test-fastb-wgicustom:
	WINE=$(PREFIX)/bin/wine test/fastb-wgicustom-gate.sh

test-fastb-wic:
	WINE=$(PREFIX)/bin/wine test/fastb-wic-gate.sh

test-fastb-winrtnet:
	WINE=$(PREFIX)/bin/wine test/fastb-winrtnet-gate.sh

test-fastb-winrtstub:
	WINE=$(PREFIX)/bin/wine test/fastb-winrtstub-gate.sh

test-fastb-winsta:
	WINE=$(PREFIX)/bin/wine test/fastb-winsta-gate.sh

test-fastb-wintab:
	WINE=$(PREFIX)/bin/wine test/fastb-wintab-gate.sh

test-fastb-wlan:
	WINE=$(PREFIX)/bin/wine test/fastb-wlan-gate.sh

test-fastb-wmiu:
	WINE=$(PREFIX)/bin/wine test/fastb-wmiu-gate.sh

test-fastb-ws2:
	WINE=$(PREFIX)/bin/wine test/fastb-ws2-gate.sh

test-fastb-wscript:
	WINE=$(PREFIX)/bin/wine test/fastb-wscript-gate.sh

test-fastb-wsd:
	WINE=$(PREFIX)/bin/wine test/fastb-wsd-gate.sh

test-fastb-wsock:
	WINE=$(PREFIX)/bin/wine test/fastb-wsock-gate.sh

test-fastb-wspool:
	WINE=$(PREFIX)/bin/wine test/fastb-wspool-gate.sh

test-fastb-wts:
	WINE=$(PREFIX)/bin/wine test/fastb-wts-gate.sh

test-fastb-xmllite:
	WINE=$(PREFIX)/bin/wine test/fastb-xmllite-gate.sh

test-fastb-xof:
	WINE=$(PREFIX)/bin/wine test/fastb-xof-gate.sh

test-fastb-xofsave:
	WINE=$(PREFIX)/bin/wine test/fastb-xofsave-gate.sh

test-fastb-zipmenu:
	WINE=$(PREFIX)/bin/wine test/fastb-zipmenu-gate.sh

test-richepara2:
	WINE=$(PREFIX)/bin/wine test/richepara2-gate.sh

test-apphelp:
	WINE=$(PREFIX)/bin/wine test/apphelp-gate.sh

test-arp:
	WINE=$(PREFIX)/bin/wine test/arp-gate.sh

test-authz:
	WINE=$(PREFIX)/bin/wine test/authz-gate.sh

test-difxapi:
	WINE=$(PREFIX)/bin/wine test/difxapi-gate.sh

test-dllclass:
	WINE=$(PREFIX)/bin/wine test/dllclass-gate.sh

test-dllhost:
	WINE=$(PREFIX)/bin/wine test/dllhost-gate.sh

test-dwmthumb:
	WINE=$(PREFIX)/bin/wine test/dwmthumb-gate.sh

test-fontsub:
	WINE=$(PREFIX)/bin/wine test/fontsub-gate.sh

test-loadperf:
	WINE=$(PREFIX)/bin/wine test/loadperf-gate.sh

test-msiexec-repair:
	WINE=$(PREFIX)/bin/wine test/msiexec-repair-gate.sh

test-newdev:
	WINE=$(PREFIX)/bin/wine test/newdev-gate.sh

test-nsi:
	WINE=$(PREFIX)/bin/wine test/nsi-gate.sh

test-pnputil:
	WINE=$(PREFIX)/bin/wine test/pnputil-gate.sh

test-setx:
	WINE=$(PREFIX)/bin/wine test/setx-gate.sh

test-taskmonth:
	WINE=$(PREFIX)/bin/wine test/taskmonth-gate.sh

test-tbs:
	WINE=$(PREFIX)/bin/wine test/tbs-gate.sh

test-wscript:
	WINE=$(PREFIX)/bin/wine test/wscript-gate.sh

# mscoree 5906 (CORE review: security): ECMA-key assemblies trusted only inside a Wine Mono installation.
test-sn-ecma:
	WINE=$(PREFIX)/bin/wine test/sn-ecma-gate.sh

# ncrypt 5907 (CORE review: security): PKCS #8 export policy, plain vs encrypted.
test-ncrypt-p8policy:
	WINE=$(PREFIX)/bin/wine test/ncrypt-p8policy-gate.sh

# ntdll 5923 (CORE review: ntdll file attributes): hidden/system of a read-only file are kept.
test-rohidden:
	WINE=$(PREFIX)/bin/wine test/rohidden-gate.sh

# server/ntdll 5927 (CORE review: server): dangling and directory symbolic links are deleted as themselves.
test-linkdel:
	WINE=$(PREFIX)/bin/wine test/linkdel-gate.sh

# wintypes 5928: the flat API contract exports report no version whose classes are missing (Word hung at its splash).
test-flatcontract:
	WINE=$(PREFIX)/bin/wine test/flatcontract-gate.sh

# windows.web 5929: HttpResponseMessage names the request it answers (Office react-native crash).
test-webreqmsg:
	WINE=$(PREFIX)/bin/wine test/webreqmsg-gate.sh
test-powershellstdio:
	WINE=$(PREFIX)/bin/wine test/powershell-stdio-gate.sh
test-portraitdesktop:
	WINE=$(PREFIX)/bin/wine test/portrait-desktop-gate.sh
test-althook:
	WINE=$(PREFIX)/bin/wine test/althook-gate.sh
test-focusswitch:
	WINE=$(PREFIX)/bin/wine test/focusswitch-gate.sh

test-kbdhkl:
	WINE=$(PREFIX)/bin/wine test/kbdhkl-gate.sh

test-linksize:
	WINE=$(PREFIX)/bin/wine test/linksize-gate.sh

test-reole:
	WINE=$(PREFIX)/bin/wine test/reole-gate.sh

test-retom:
	WINE=$(PREFIX)/bin/wine test/retom-gate.sh

test-olelink:
	WINE=$(PREFIX)/bin/wine test/olelink-gate.sh

test-stgswmr:
	WINE=$(PREFIX)/bin/wine test/stgswmr-gate.sh

test-colorglyph:
	WINE=$(PREFIX)/bin/wine test/colorglyph-gate.sh

test-uspfeat:
	WINE=$(PREFIX)/bin/wine test/uspfeat-gate.sh

test-eot:
	WINE=$(PREFIX)/bin/wine test/eot-gate.sh

test-wsproto:
	WINE=$(PREFIX)/bin/wine test/wsproto-gate.sh

test-adoauth:
	WINE=$(PREFIX)/bin/wine test/adoauth-gate.sh

test-stdmarshalex:
	WINE=$(PREFIX)/bin/wine test/stdmarshalex-gate.sh

test-kmount:
	WINE=$(PREFIX)/bin/wine test/kmount-gate.sh

test-wicfast:
	WINE=$(PREFIX)/bin/wine test/wicfast-gate.sh

test-pm3:
	WINE=$(PREFIX)/bin/wine test/pm3-gate.sh

test-wmiclass:
	WINE=$(PREFIX)/bin/wine test/wmiclass-gate.sh

test-doctypeent:
	WINE=$(PREFIX)/bin/wine test/doctypeent-gate.sh

test-gdipprop:
	WINE=$(PREFIX)/bin/wine test/gdipprop-gate.sh

test-ps2csa:
	WINE=$(PREFIX)/bin/wine test/ps2csa-gate.sh

test-qcaex:
	WINE=$(PREFIX)/bin/wine test/qcaex-gate.sh

test-smallexp:
	WINE=$(PREFIX)/bin/wine test/smallexp-gate.sh

test-d3d9share:
	WINE=$(PREFIX)/bin/wine test/d3d9share-gate.sh

test-procdpi:
	WINE=$(PREFIX)/bin/wine test/procdpi-gate.sh

test-stacklimit:
	WINE=$(PREFIX)/bin/wine test/stacklimit-gate.sh

test-msirmuse:
	WINE=$(PREFIX)/bin/wine test/msirmuse-gate.sh

test-deskread:
	WINE=$(PREFIX)/bin/wine test/deskread-gate.sh

test-relocheap:
	WINE=$(PREFIX)/bin/wine test/relocheap-gate.sh

test-noalias:
	WINE=$(PREFIX)/bin/wine test/noalias-gate.sh

test-readview:
	WINE=$(PREFIX)/bin/wine test/readview-gate.sh

test-wowstack:
	WINE=$(PREFIX)/bin/wine test/wowstack-gate.sh

test-rootwow:
	WINE=$(PREFIX)/bin/wine test/rootwow-gate.sh

test-sparse:
	WINE=$(PREFIX)/bin/wine test/sparse-gate.sh

test-comuser:
	WINE=$(PREFIX)/bin/wine test/comuser-gate.sh

test-pkgverb:
	WINE=$(PREFIX)/bin/wine test/pkgverb-gate.sh

test-unknowntype:
	WINE=$(PREFIX)/bin/wine test/unknowntype-gate.sh

test-adoodbc:
	WINE=$(PREFIX)/bin/wine test/adoodbc-gate.sh

test-adoparam:
	WINE=$(PREFIX)/bin/wine test/adoparam-gate.sh

test-vbsnothing:
	WINE=$(PREFIX)/bin/wine test/vbsnothing-gate.sh

test-sysappx:
	WINE=$(PREFIX)/bin/wine test/sysappx-gate.sh

test-adoupdate:
	WINE=$(PREFIX)/bin/wine test/adoupdate-gate.sh

test-adolong:
	WINE=$(PREFIX)/bin/wine test/adolong-gate.sh

test-junction:
	WINE=$(PREFIX)/bin/wine test/junction-gate.sh

test-wmienum:
	WINE=$(PREFIX)/bin/wine test/wmienum-gate.sh

test-tom2range:
	WINE=$(PREFIX)/bin/wine test/tom2range-gate.sh

test-wicimageenc:
	WINE=$(PREFIX)/bin/wine test/wicimageenc-gate.sh

test-xproc-still:
	WINE=$(PREFIX)/bin/wine test/xproc-still-gate.sh

test-ctrlrange:
	WINE=$(PREFIX)/bin/wine test/ctrlrange-gate.sh

test-wuianim:
	WINE=$(PREFIX)/bin/wine test/wuianim-gate.sh

test-displaysvc:
	WINE=$(PREFIX)/bin/wine test/displaysvc-gate.sh

test-tomfont2:
	WINE=$(PREFIX)/bin/wine test/tomfont2-gate.sh

test-somschema:
	WINE=$(PREFIX)/bin/wine test/somschema-gate.sh

test-wmipoly:
	WINE=$(PREFIX)/bin/wine test/wmipoly-gate.sh

test-blockmap:
	WINE=$(PREFIX)/bin/wine test/blockmap-gate.sh

test-appxwriter:
	WINE=$(PREFIX)/bin/wine test/appxwriter-gate.sh

test-secresults:
	WINE=$(PREFIX)/bin/wine test/secresults-gate.sh

test-sdbfile:
	WINE=$(PREFIX)/bin/wine test/sdbfile-gate.sh

test-appxbundlew:
	WINE=$(PREFIX)/bin/wine test/appxbundlew-gate.sh

test-txfail:
	WINE=$(PREFIX)/bin/wine test/txfail-gate.sh

test-engpaths:
	WINE=$(PREFIX)/bin/wine test/engpaths-gate.sh

test-kmtqueued:
	WINE=$(PREFIX)/bin/wine test/kmtqueued-gate.sh

test-dpvsession:
	WINE=$(PREFIX)/bin/wine test/dpvsession-gate.sh

test-tcnone:
	WINE=$(PREFIX)/bin/wine test/tcnone-gate.sh

test-mmiotemp:
	WINE=$(PREFIX)/bin/wine test/mmiotemp-gate.sh

test-clrtools:
	WINE=$(PREFIX)/bin/wine test/clrtools-gate.sh

test-dnssd:
	WINE=$(PREFIX)/bin/wine test/dnssd-gate.sh

test-dnsupdate:
	WINE=$(PREFIX)/bin/wine test/dnsupdate-gate.sh

test-clickonce:
	WINE=$(PREFIX)/bin/wine test/clickonce-gate.sh

test-restartreason:
	WINE=$(PREFIX)/bin/wine test/restartreason-gate.sh

test-dumpread:
	WINE=$(PREFIX)/bin/wine test/dumpread-gate.sh

test-monoagent:
	WINE=$(PREFIX)/bin/wine test/monoagent-gate.sh

test-wsdprovider:
	WINE=$(PREFIX)/bin/wine test/wsdprovider-gate.sh

test-lpcport:
	WINE=$(PREFIX)/bin/wine test/lpcport-gate.sh

test-wsfault:
	WINE=$(PREFIX)/bin/wine test/wsfault-gate.sh

test-icmadmin:
	WINE=$(PREFIX)/bin/wine test/icmadmin-gate.sh

test-ppcontrol:
	WINE=$(PREFIX)/bin/wine test/ppcontrol-gate.sh

test-voicename:
	WINE=$(PREFIX)/bin/wine test/voicename-gate.sh

test-shelllibrary:
	WINE=$(PREFIX)/bin/wine test/shelllibrary-gate.sh

test-regnotifyapc:
	WINE=$(PREFIX)/bin/wine test/regnotifyapc-gate.sh

test-iconcross:
	WINE=$(PREFIX)/bin/wine test/iconcross-gate.sh

test-icuconv:
	WINE=$(PREFIX)/bin/wine test/icuconv-gate.sh

test-dwmframe:
	WINE=$(PREFIX)/bin/wine test/dwmframe-gate.sh

test-devnodeprops:
	WINE=$(PREFIX)/bin/wine test/devnodeprops-gate.sh

test-sha3:
	WINE=$(PREFIX)/bin/wine test/sha3-gate.sh

test-bcryptkdf:
	WINE=$(PREFIX)/bin/wine test/bcryptkdf-gate.sh

test-schemedesktop:
	WINE=$(PREFIX)/bin/wine test/schemedesktop-gate.sh

test-aesecb:
	WINE=$(PREFIX)/bin/wine test/aesecb-gate.sh

test-sapiaudioout:
	WINE=$(PREFIX)/bin/wine test/sapiaudioout-gate.sh

test-keynames:
	WINE=$(PREFIX)/bin/wine test/keynames-gate.sh

test-unixpipe:
	WINE=$(PREFIX)/bin/wine test/unixpipe-gate.sh

test-xproc-child:
	WINE=$(PREFIX)/bin/wine test/xproc-child-gate.sh

test-d2dblend:
	WINE=$(PREFIX)/bin/wine test/d2dblend-gate.sh

test-gdipemfplus:
	WINE=$(PREFIX)/bin/wine test/gdipemfplus-gate.sh

test-gdipoutline:
	WINE=$(PREFIX)/bin/wine test/gdipoutline-gate.sh

test-icmdefaults:
	WINE=$(PREFIX)/bin/wine test/icmdefaults-gate.sh

test-pscff:
	WINE=$(PREFIX)/bin/wine test/pscff-gate.sh

test-ttsvoice:
	WINE=$(PREFIX)/bin/wine test/ttsvoice-gate.sh

test-acmfilter:
	WINE=$(PREFIX)/bin/wine test/acmfilter-gate.sh


test-wmsynth:
	WINE=$(PREFIX)/bin/wine test/wmsynth-gate.sh

test-aviclip:
	WINE=$(PREFIX)/bin/wine test/aviclip-gate.sh

test-ofnpreview:
	WINE=$(PREFIX)/bin/wine test/ofnpreview-gate.sh

test-mciconfig:
	WINE=$(PREFIX)/bin/wine test/mciconfig-gate.sh

test-wmedia:
	WINE=$(PREFIX)/bin/wine test/wmedia-gate.sh

test-wmeditor:
	WINE=$(PREFIX)/bin/wine test/wmeditor-gate.sh

test-qcaprender:
	WINE=$(PREFIX)/bin/wine test/qcaprender-gate.sh

test-wmwriter:
	WINE=$(PREFIX)/bin/wine test/wmwriter-gate.sh

test-wmencode:
	WINE=$(PREFIX)/bin/wine test/wmencode-gate.sh

test-winusbapi:
	WINE=$(PREFIX)/bin/wine test/winusbapi-gate.sh

test-winusb:
	WINE=$(PREFIX)/bin/wine test/winusb-gate.sh

test-wmtiming:
	WINE=$(PREFIX)/bin/wine test/wmtiming-gate.sh

test-mpris:
	WINE=$(PREFIX)/bin/wine test/mpris-gate.sh

test-appcmdkeys:
	WINE=$(PREFIX)/bin/wine test/appcmdkeys-gate.sh

test-connrefused:
	WINE=$(PREFIX)/bin/wine test/connrefused-gate.sh

test-cmdlabelecho:
	WINE=$(PREFIX)/bin/wine test/cmdlabelecho-gate.sh

test-glhiddenframe:
	WINE=$(PREFIX)/bin/wine test/glhiddenframe-gate.sh

test-d3d11msaaflags:
	WINE=$(PREFIX)/bin/wine test/d3d11msaaflags-gate.sh

test-taskviewmove:
	WINE=$(PREFIX)/bin/wine test/taskviewmove-gate.sh

test-childbutton:
	WINE=$(PREFIX)/bin/wine test/childbutton-gate.sh

test-hlslcopyprop:
	WINE=$(PREFIX)/bin/wine test/hlslcopyprop-gate.sh

test-wtsunload:
	WINE=$(PREFIX)/bin/wine test/wtsunload-gate.sh

test-lconvwide:
	WINE=$(PREFIX)/bin/wine test/lconvwide-gate.sh

test-fontsnoz:
	WINE=$(PREFIX)/bin/wine test/fontsnoz-gate.sh

test-bgplayer:
	WINE=$(PREFIX)/bin/wine test/bgplayer-gate.sh

test-cffoutline:
	WINE=$(PREFIX)/bin/wine test/cffoutline-gate.sh

test-dwoutline:
	WINE=$(PREFIX)/bin/wine test/dwoutline-gate.sh

test-heifdecode:
	WINE=$(PREFIX)/bin/wine test/heifdecode-gate.sh

test-kbdcues:
	WINE=$(PREFIX)/bin/wine test/kbdcues-gate.sh

test-glforeign:
	WINE=$(PREFIX)/bin/wine test/glforeign-gate.sh

test-composited:
	WINE=$(PREFIX)/bin/wine test/composited-gate.sh

test-printguard:
	WINE=$(PREFIX)/bin/wine test/printguard-gate.sh

test-linkprocess:
	WINE=$(PREFIX)/bin/wine test/linkprocess-gate.sh

test-evrpresenter:
	WINE=$(PREFIX)/bin/wine test/evrpresenter-gate.sh

test-fontsfolder:
	WINE=$(PREFIX)/bin/wine test/fontsfolder-gate.sh

test-fontsshared:
	WINE=$(PREFIX)/bin/wine test/fontsshared-gate.sh

test-wbemrefresher:
	WINE=$(PREFIX)/bin/wine test/wbemrefresher-gate.sh

test-impersonatename:
	WINE=$(PREFIX)/bin/wine test/impersonatename-gate.sh

test-dwbitmapmode:
	WINE=$(PREFIX)/bin/wine test/dwbitmapmode-gate.sh

test-layeredchild:
	WINE=$(PREFIX)/bin/wine test/layeredchild-gate.sh

test-rpcicarus:
	WINE=$(PREFIX)/bin/wine test/rpcicarus-gate.sh

test-svcsetstatus:
	WINE=$(PREFIX)/bin/wine test/svcsetstatus-gate.sh

test-setgidtree:
	WINE=$(PREFIX)/bin/wine test/setgidtree-gate.sh

test-hkcuwow32:
	WINE=$(PREFIX)/bin/wine test/hkcuwow32-gate.sh

test-uiadisconnectall:
	WINE=$(PREFIX)/bin/wine test/uiadisconnectall-gate.sh

test-txdrawscroll:
	WINE=$(PREFIX)/bin/wine test/txdrawscroll-gate.sh

test-noassoc:
	WINE=$(PREFIX)/bin/wine test/noassoc-gate.sh

test-fxinline:
	WINE=$(PREFIX)/bin/wine test/fxinline-gate.sh

test-wowattach:
	WINE=$(PREFIX)/bin/wine test/wowattach-gate.sh

test-k32gpa:
	WINE=$(PREFIX)/bin/wine test/k32gpa-gate.sh

test-conpty:
	WINE=$(PREFIX)/bin/wine test/conpty-gate.sh

test-xmlappend:
	WINE=$(PREFIX)/bin/wine test/xmlappend-gate.sh

test-msifont:
	WINE=$(PREFIX)/bin/wine test/msifont-gate.sh

test-regexpandpair:
	WINE=$(PREFIX)/bin/wine test/regexpandpair-gate.sh

test-vcompsched:
	WINE=$(PREFIX)/bin/wine test/vcompsched-gate.sh
