#!/bin/sh
# Stand-in for /usr/bin/sg-wam-msal (webauthcore-gate.sh; SG_WAM_HELPER names
# this file). Reads the one-line JSON request on stdin, appends it to the file
# SG_WAM_REQLOG, and answers with one `SGWAM-RESULT:{json}` line chosen by the
# request's login_hint (one helper serves every case of the probe):
#   interact@...  -> ok:false, interaction_required
#   deny@...      -> ok:false, error access_denied
#   cancel@...    -> ok:false, a description containing "cancel"
#   usercancelled@... -> ok:false, the description "User Cancelled"
#   anything else -> ok:true, a token and account
req=$(cat)
[ -n "${SG_WAM_REQLOG:-}" ] && printf '%s\n' "$req" >> "$SG_WAM_REQLOG"
case "$req" in
*'"login_hint":"interact@'*)
    echo 'SGWAM-RESULT:{"ok":false,"error":"interaction_required","error_description":"AADSTS50058 sign in again","interaction_required":true}' ;;
*'"login_hint":"deny@'*)
    echo 'SGWAM-RESULT:{"ok":false,"error":"access_denied","error_description":"The user declined","interaction_required":false}' ;;
*'"login_hint":"usercancelled@'*)
    echo 'SGWAM-RESULT:{"ok":false,"error":"user_error","error_description":"User Cancelled","interaction_required":false}' ;;
*'"login_hint":"cancel@'*)
    echo 'SGWAM-RESULT:{"ok":false,"error":"user_error","error_description":"The user did cancel the sign-in","interaction_required":false}' ;;
*)
    echo 'SGWAM-RESULT:{"ok":true,"result":{"access_token":"tok-ABC123","token_type":"Bearer","expires_in":3599,"id_token":"idtok.payload.sig","client_info":"eyJ1aWQiOiJ4In0","scope":"User.Read openid","account":{"home_account_id":"uid.utid","username":"jane@contoso.com"}}}' ;;
esac
