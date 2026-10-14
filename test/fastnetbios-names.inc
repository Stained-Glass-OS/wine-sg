/* NCBADDNAME, NCBADDGRNAME: the name goes to the local name table of the adapter and gets
 * a name number, 0x02 to 0xfe.  A name of the table cannot be added again. */
static UCHAR nbAddName(NetBIOSAdapter *adapter, PNCB ncb, BOOL group)
{
    UCHAR ret = NRC_GOODRET, i, num;

    if (!adapter) return NRC_BRIDGE;
    if (!ncb) return NRC_INVADDRESS;
    /* a name cannot start with '*' or 0 */
    if (ncb->ncb_name[0] == '*' || ncb->ncb_name[0] == 0) return NRC_NOWILD;

    EnterCriticalSection(&adapter->cs);
    for (i = 0; i < adapter->nameCount; i++)
        if (!memcmp(adapter->names[i].name, ncb->ncb_name, NCBNAMSZ)) break;
    if (i < adapter->nameCount)
    {
        /* the same name is a duplicate, unless both are group names */
        if (group && (adapter->names[i].flags & GROUP_NAME)) ncb->ncb_num = adapter->names[i].num;
#ifndef SG_MUTANT_NB_DUP
        else ret = NRC_DUPNAME;
#endif
    }
#ifndef SG_MUTANT_NB_FULL
    else if (adapter->nameCount >= NB_MAX_NAMES) ret = NRC_NAMTFUL;
#endif
    else
    {
        /* the lowest number that is not used */
#ifdef SG_MUTANT_NB_NUM
        for (num = 1;; num++)
#else
        for (num = 2;; num++)
#endif
        {
            for (i = 0; i < adapter->nameCount; i++) if (adapter->names[i].num == num) break;
            if (i == adapter->nameCount) break;
        }
        memcpy(adapter->names[adapter->nameCount].name, ncb->ncb_name, NCBNAMSZ);
        adapter->names[adapter->nameCount].num = num;
        adapter->names[adapter->nameCount].flags = REGISTERED | (group ? GROUP_NAME : UNIQUE_NAME);
        adapter->nameCount++;
        ncb->ncb_num = num;
    }
    LeaveCriticalSection(&adapter->cs);
    return ret;
}

/* NCBDELNAME: a name with a session is kept until the session ends, by the state DEREGISTERED */
static UCHAR nbDelName(NetBIOSAdapter *adapter, PNCB ncb)
{
    UCHAR ret = NRC_GOODRET, i;
    DWORD s;

    if (!adapter) return NRC_BRIDGE;
    if (!ncb) return NRC_INVADDRESS;
    if (ncb->ncb_name[0] == '*' || ncb->ncb_name[0] == 0) return NRC_NOWILD;

    EnterCriticalSection(&adapter->cs);
    for (i = 0; i < adapter->nameCount; i++)
        if (!memcmp(adapter->names[i].name, ncb->ncb_name, NCBNAMSZ)) break;
    if (i == adapter->nameCount) ret = NRC_NOWILD;
    else
    {
        BOOL in_use = FALSE;

        for (s = 0; s < adapter->sessionsLen; s++)
            if (adapter->sessions[s].inUse && !memcmp(adapter->sessions[s].local_name, ncb->ncb_name, NCBNAMSZ))
                in_use = TRUE;
#ifdef SG_MUTANT_NB_INUSE
        in_use = FALSE;
#endif
        if (in_use)
        {
            adapter->names[i].flags = (adapter->names[i].flags & GROUP_NAME) | DEREGISTERED;
            ret = NRC_INUSE;
        }
        else
        {
            ncb->ncb_num = adapter->names[i].num;
            memmove(&adapter->names[i], &adapter->names[i + 1], (adapter->nameCount - i - 1) * sizeof(adapter->names[0]));
            adapter->nameCount--;
        }
    }
    LeaveCriticalSection(&adapter->cs);
    return ret;
}

/* the names of the local table follow the names the transport reports for the adapter itself */
static void nbAStatAddNames(NetBIOSAdapter *adapter, PNCB ncb)
{
    PADAPTER_STATUS astat = (PADAPTER_STATUS)ncb->ncb_buffer;
    PNAME_BUFFER dst = (PNAME_BUFFER)(astat + 1) + astat->name_count;
    UCHAR i;

    EnterCriticalSection(&adapter->cs);
    for (i = 0; i < adapter->nameCount; i++, dst++)
    {
        if ((PUCHAR)(dst + 1) > (PUCHAR)ncb->ncb_buffer + ncb->ncb_length) break;
        memcpy(dst->name, adapter->names[i].name, NCBNAMSZ);
        dst->name_num = adapter->names[i].num;
        dst->name_flags = adapter->names[i].flags;
        astat->name_count++;
    }
    LeaveCriticalSection(&adapter->cs);
}

