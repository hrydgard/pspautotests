.set noreorder

#include "pspimport.s"

# Declared here rather than taken from the SDK's stubs, which have broken function counts for sceNet.

IMPORT_START "sceNet",0x00090000
IMPORT_FUNC	"sceNet",0x39AF39A6,sceNetInit
IMPORT_FUNC	"sceNet",0x281928A9,sceNetTerm
IMPORT_FUNC	"sceNet",0x0BF0A3AE,sceNetGetLocalEtherAddr

IMPORT_START "sceNetAdhoc",0x00090000
IMPORT_FUNC	"sceNetAdhoc",0xE1D621D7,sceNetAdhocInit
IMPORT_FUNC	"sceNetAdhoc",0xA62C6F57,sceNetAdhocTerm
IMPORT_FUNC	"sceNetAdhoc",0x6F92741B,sceNetAdhocPdpCreate
IMPORT_FUNC	"sceNetAdhoc",0xABED3790,sceNetAdhocPdpSend
IMPORT_FUNC	"sceNetAdhoc",0xDFE53E03,sceNetAdhocPdpRecv
IMPORT_FUNC	"sceNetAdhoc",0x7F27BB5E,sceNetAdhocPdpDelete
IMPORT_FUNC	"sceNetAdhoc",0xC7C1FC57,sceNetAdhocGetPdpStat
IMPORT_FUNC	"sceNetAdhoc",0x157E6225,sceNetAdhocPtpClose
IMPORT_FUNC	"sceNetAdhoc",0x4DA4C788,sceNetAdhocPtpSend
IMPORT_FUNC	"sceNetAdhoc",0x877F6D66,sceNetAdhocPtpOpen
IMPORT_FUNC	"sceNetAdhoc",0x8BEA2B3E,sceNetAdhocPtpRecv
IMPORT_FUNC	"sceNetAdhoc",0x9DF81198,sceNetAdhocPtpAccept
IMPORT_FUNC	"sceNetAdhoc",0xE08BDAC1,sceNetAdhocPtpListen
IMPORT_FUNC	"sceNetAdhoc",0xFC6FC07B,sceNetAdhocPtpConnect
IMPORT_FUNC	"sceNetAdhoc",0x9AC2EEAC,sceNetAdhocPtpFlush
IMPORT_FUNC	"sceNetAdhoc",0xB9685118,sceNetAdhocGetPtpStat
IMPORT_FUNC	"sceNetAdhoc",0x73BFD52D,sceNetAdhocSetSocketAlert
IMPORT_FUNC	"sceNetAdhoc",0x4D2CE199,sceNetAdhocGetSocketAlert
IMPORT_FUNC	"sceNetAdhoc",0x7A662D6B,sceNetAdhocPollSocket
IMPORT_FUNC	"sceNetAdhoc",0x7F75C338,sceNetAdhocGameModeCreateMaster
IMPORT_FUNC	"sceNetAdhoc",0x3278AB0C,sceNetAdhocGameModeCreateReplica
IMPORT_FUNC	"sceNetAdhoc",0x98C204C8,sceNetAdhocGameModeUpdateMaster
IMPORT_FUNC	"sceNetAdhoc",0xFA324B4E,sceNetAdhocGameModeUpdateReplica
IMPORT_FUNC	"sceNetAdhoc",0xA0229362,sceNetAdhocGameModeDeleteMaster
IMPORT_FUNC	"sceNetAdhoc",0x0B2228E9,sceNetAdhocGameModeDeleteReplica

IMPORT_START "sceNetAdhocctl",0x00090000
IMPORT_FUNC	"sceNetAdhocctl",0xE26F226E,sceNetAdhocctlInit
IMPORT_FUNC	"sceNetAdhocctl",0x9D689E13,sceNetAdhocctlTerm
IMPORT_FUNC	"sceNetAdhocctl",0x20B317A0,sceNetAdhocctlAddHandler
IMPORT_FUNC	"sceNetAdhocctl",0x6402490B,sceNetAdhocctlDelHandler
IMPORT_FUNC	"sceNetAdhocctl",0x34401D65,sceNetAdhocctlDisconnect
IMPORT_FUNC	"sceNetAdhocctl",0x0AD043ED,sceNetAdhocctlConnect
IMPORT_FUNC	"sceNetAdhocctl",0x08FFF7A0,sceNetAdhocctlScan
IMPORT_FUNC	"sceNetAdhocctl",0x75ECD386,sceNetAdhocctlGetState
IMPORT_FUNC	"sceNetAdhocctl",0xDED9D28E,sceNetAdhocctlGetParameter
IMPORT_FUNC	"sceNetAdhocctl",0x81AEE1BE,sceNetAdhocctlGetScanInfo
IMPORT_FUNC	"sceNetAdhocctl",0xEC0635C1,sceNetAdhocctlCreate
IMPORT_FUNC	"sceNetAdhocctl",0xE162CB14,sceNetAdhocctlGetPeerList
IMPORT_FUNC	"sceNetAdhocctl",0x362CBE8F,sceNetAdhocctlGetAdhocId
IMPORT_FUNC	"sceNetAdhocctl",0x8DB83FDC,sceNetAdhocctlGetPeerInfo
IMPORT_FUNC	"sceNetAdhocctl",0x5E7F79C9,sceNetAdhocctlJoin
IMPORT_FUNC	"sceNetAdhocctl",0x8916C003,sceNetAdhocctlGetNameByAddr
IMPORT_FUNC	"sceNetAdhocctl",0x99560ABE,sceNetAdhocctlGetAddrByName
IMPORT_FUNC	"sceNetAdhocctl",0xA5C055CE,sceNetAdhocctlCreateEnterGameMode
IMPORT_FUNC	"sceNetAdhocctl",0x1FF89745,sceNetAdhocctlJoinEnterGameMode
IMPORT_FUNC	"sceNetAdhocctl",0xCF8E084D,sceNetAdhocctlExitGameMode
IMPORT_FUNC	"sceNetAdhocctl",0x5A014CE0,sceNetAdhocctlGetGameModeInfo
IMPORT_FUNC	"sceNetAdhocctl",0xB0B80E80,sceNetAdhocctlCreateEnterGameModeMin

IMPORT_START "sceNetAdhocMatching",0x00090000
IMPORT_FUNC	"sceNetAdhocMatching",0x2A2A1E07,sceNetAdhocMatchingInit
IMPORT_FUNC	"sceNetAdhocMatching",0x7945ECDA,sceNetAdhocMatchingTerm
IMPORT_FUNC	"sceNetAdhocMatching",0xCA5EDA6F,sceNetAdhocMatchingCreate
IMPORT_FUNC	"sceNetAdhocMatching",0x93EF3843,sceNetAdhocMatchingStart
IMPORT_FUNC	"sceNetAdhocMatching",0x32B156B3,sceNetAdhocMatchingStop
IMPORT_FUNC	"sceNetAdhocMatching",0xF16EAF4F,sceNetAdhocMatchingDelete
IMPORT_FUNC	"sceNetAdhocMatching",0x5E3D4B79,sceNetAdhocMatchingSelectTarget
IMPORT_FUNC	"sceNetAdhocMatching",0xEA3C6108,sceNetAdhocMatchingCancelTarget
IMPORT_FUNC	"sceNetAdhocMatching",0xB5D96C2A,sceNetAdhocMatchingGetHelloOpt
IMPORT_FUNC	"sceNetAdhocMatching",0xB58E61B7,sceNetAdhocMatchingSetHelloOpt
IMPORT_FUNC	"sceNetAdhocMatching",0xC58BCD9E,sceNetAdhocMatchingGetMembers
IMPORT_FUNC	"sceNetAdhocMatching",0xF79472D7,sceNetAdhocMatchingSendData
IMPORT_FUNC	"sceNetAdhocMatching",0x40F8F435,sceNetAdhocMatchingGetPoolMaxAlloc
IMPORT_FUNC	"sceNetAdhocMatching",0x9C5CFB7D,sceNetAdhocMatchingGetPoolStat
IMPORT_FUNC	"sceNetAdhocMatching",0x8F58BEDF,sceNetAdhocMatchingCancelTargetWithOpt
IMPORT_FUNC	"sceNetAdhocMatching",0xEC19337D,sceNetAdhocMatchingAbortSendData

IMPORT_START "sceWlanDrv",0x00090000
IMPORT_FUNC	"sceWlanDrv",0x0C622081,sceWlanGetEtherAddr
IMPORT_FUNC	"sceWlanDrv",0xD7763699,sceWlanGetSwitchState

IMPORT_START "sceNetInet",0x00090000
IMPORT_FUNC	"sceNetInet",0x17943399,sceNetInetInit
IMPORT_FUNC	"sceNetInet",0xA9ED66B9,sceNetInetTerm
IMPORT_FUNC	"sceNetInet",0x8B7B220F,sceNetInetSocket
IMPORT_FUNC	"sceNetInet",0x1A33F9AE,sceNetInetBind
IMPORT_FUNC	"sceNetInet",0x05038FC7,sceNetInetSendto
IMPORT_FUNC	"sceNetInet",0xC91142E4,sceNetInetRecvfrom
IMPORT_FUNC	"sceNetInet",0x8D7284EA,sceNetInetClose
IMPORT_FUNC	"sceNetInet",0x5BE8D595,sceNetInetSelect
IMPORT_FUNC	"sceNetInet",0xFBABE411,sceNetInetGetErrno
IMPORT_FUNC	"sceNetInet",0xD10A1A7A,sceNetInetListen
IMPORT_FUNC	"sceNetInet",0x410B34AA,sceNetInetConnect
IMPORT_FUNC	"sceNetInet",0xDB094E1B,sceNetInetAccept
IMPORT_FUNC	"sceNetInet",0x7AA671BC,sceNetInetSend
IMPORT_FUNC	"sceNetInet",0xCDA85C99,sceNetInetRecv

IMPORT_START "sceNetApctl",0x00090000
IMPORT_FUNC	"sceNetApctl",0xE2F91F9B,sceNetApctlInit
IMPORT_FUNC	"sceNetApctl",0xB3EDD0EC,sceNetApctlTerm
IMPORT_FUNC	"sceNetApctl",0xCFB957C6,sceNetApctlConnect
IMPORT_FUNC	"sceNetApctl",0x24FE91A1,sceNetApctlDisconnect
IMPORT_FUNC	"sceNetApctl",0x5DEAC81B,sceNetApctlGetState

IMPORT_START "sceNetAdhocDiscover",0x00090000
IMPORT_FUNC	"sceNetAdhocDiscover",0x941B3877,sceNetAdhocDiscoverInitStart
IMPORT_FUNC	"sceNetAdhocDiscover",0x52DE1B97,sceNetAdhocDiscoverUpdate
IMPORT_FUNC	"sceNetAdhocDiscover",0x944DDBC6,sceNetAdhocDiscoverGetStatus
IMPORT_FUNC	"sceNetAdhocDiscover",0xA2246614,sceNetAdhocDiscoverTerm
IMPORT_FUNC	"sceNetAdhocDiscover",0xF7D13214,sceNetAdhocDiscoverStop
IMPORT_FUNC	"sceNetAdhocDiscover",0xA423A21B,sceNetAdhocDiscoverRequestSuspend
