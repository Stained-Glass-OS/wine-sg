# Office with a work or school account

Patches 1477-1479 (with `sg-wam-msal` in sg-session) let Microsoft 365 Apps sign a work or school
account in and license itself, with the sign-in page in the person's own browser.

## How it fits together

```
Word -> OneAuth/MSAL (Office's own) -> Windows.Security.Authentication.Web.Core   (1479, this repo)
                                          |  token request: authority, client id, scopes, LoginHint
                                          v
                                      /usr/bin/sg-wam-msal   (sg-session; Python, Microsoft's MSAL)
                                          |  xdg-open <sign-in page>  ........> the person's browser
                                          |  <- ms-appx-web://Microsoft.AAD.BrokerPlugin/<client id>?code=...
                                          v        (sg-wam-redirect, the scheme's handler)
                                      token + id token + client info -> back up as a WebTokenResponse
```

Passwords never pass through wine-sg. The program is `SG_WAM_HELPER` when that names an absolute Unix
path (the test suite's stub), otherwise `/usr/bin/sg-wam-msal`.

## What Office's MSAL checks in the answer (found the hard way)

Office throws the whole sign-in away, with a generic "Something went wrong" page, when the answer is
not exactly right. Its own log says why (see below). In order of discovery:

1. **`TokenExpiresOn`** (a response property) is a decimal count of *seconds since 1601*. The parser
   (`FromUniversalStringToTimePoint`) `strtoll`s it, requires it to exceed 11644473600, subtracts that
   and scales by 10^7. ISO dates, RFC 1123 dates and Unix seconds are "Invalid universal time";
   milliseconds and 100 ns ticks parse but give a date `gmtime_s` rejects. Either way the token
   "is expired" and licensing never starts.
2. **`wamcompat_id_token`, `wamcompat_client_info`, `wamcompat_scopes`** accompany every answer
   ("Missing wamcompat_client_info in WAM case"). A silent answer served from MSAL's cache has none of
   the three unless the program adds them.
3. **The login hint** of a silent request is `O.<base64 protobuf>` (object id, tenant id), not an
   e-mail address, and the web account's id is the MSAL home account id (`uid.utid`).
4. **wininet** must accept `INTERNET_OPTION_LISTEN_TIMEOUT` (1477) or OneAuth's HTTP calls fail with
   12009 (`ERROR_INTERNET_INVALID_OPTION`).

Licensing needs nothing more than an Entra access token for `https://officeapps.live.com` issued to
Office's client id (`d3590ed6-52b3-4102-aeff-aad2292ab01c`). **It does not need a Microsoft-account
device ticket**: those requests (`licensing.m365.svc.cloud.microsoft` through
`OnlineIdSystemAuthenticator`) are Office's *pre-sign-in* probe. When the identity's ticket could not be
loaded (point 1) Office falls back to that device path and shows "can't find your license", which
looks like a licensing fault and is not one. 1478 only makes that probe behave as the API says.

## Reading Office's log

Office writes its own trace, readable text, to
`%LOCALAPPDATA%\Temp\Diagnostics\WINWORD\Primary*.log` (tab-separated; lines other than
`SendEvent {json}` are the trace, including every `OneAuth log` and `MSAL` message with the
reason a token was refused). Grep it for `Invalid`, `Missing`, `expired`, `gmtime`.
`Licensing.*` events (`LoadIdentityTicketInSignInProvider`, `GetEntitlements`) show how far licensing got.

## Open: Word crashes in its React Native UI

With sign-in and licensing working, Word crashes shortly after it draws its UI: on the first-run
dialogs (license agreement, privacy notice) and on the Start screen, and within seconds of opening a
document (`HKCU\Software\Microsoft\Office\16.0\Word\Options\DisableBootToOfficeStart=1` skips the Start
screen). It is an Office fail-fast (`mso20win32client`, called through `react-native-win32` /
`mso40uiwin32client`; the exception code is an assertion tag that differs per crash), not part of the
sign-in. Not yet diagnosed.

## Not done

* A personal Microsoft account (`consumers`, `MBI_SSL_SHORT`): silent requests answer "interaction
  required".
* `FindAccountAsync` by id finds nothing; accounts come from `FindAllAccountsAsync`.
* A tenant that requires a compliant or joined device (Conditional Access) refuses the token; there
  is no device identity.
