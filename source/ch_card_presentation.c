#include <carryhandle/ch_card_presentation.h>
#include <carryhandle/ch_memcard.h>

#include <stdint.h>
#include <string.h>


/*
 * Offsets relative to presentation_offset.
 *
 * The host-side CARD encoder owns this exact byte format.
 */
#define CH_CARD_BANNER_REL_OFFSET          0u
#define CH_CARD_BANNER_TLUT_REL_OFFSET  3072u
#define CH_CARD_ICON_REL_OFFSET          3584u
#define CH_CARD_ICON_TLUT_REL_OFFSET     4608u
#define CH_CARD_COMMENT_REL_OFFSET       5120u


static bool presentationStatusMatches(
    const card_stat *status,
    u32 presentation_offset
)
{
    unsigned int banner_format;
    unsigned int icon_format;
    unsigned int icon_speed;

    if (!status)
    {
        return false;
    }

    /*
     * Do not use CARD_GetIconSpeed().
     *
     * The libogc2 header used during CarryHandle extraction contains
     * an incorrect mask expression in that macro. Read the packed
     * two-bit field directly.
     */
    banner_format =
        (unsigned int)(
            status->banner_fmt
            & CARD_BANNER_MASK
        );

    icon_format =
        (unsigned int)(
            status->icon_fmt
            & CARD_ICON_MASK
        );

    icon_speed =
        (unsigned int)(
            status->icon_speed
            & CARD_SPEED_MASK
        );

    return
        banner_format == CARD_BANNER_CI
        && status->icon_addr
            == presentation_offset
        && icon_format == CARD_ICON_CI
        && icon_speed == CARD_SPEED_SLOW
        && status->comment_addr
            == presentation_offset
                + CH_CARD_COMMENT_REL_OFFSET;
}


static bool presentationDerivedOffsetsMatch(
    const card_stat *status,
    u32 presentation_offset
)
{
    if (!status)
    {
        return false;
    }

    return
        status->offset_banner
            == presentation_offset
                + CH_CARD_BANNER_REL_OFFSET

        && status->offset_banner_tlut
            == presentation_offset
                + CH_CARD_BANNER_TLUT_REL_OFFSET

        && status->offset_icon[0]
            == presentation_offset
                + CH_CARD_ICON_REL_OFFSET

        && status->offset_icon_tlut[0]
            == presentation_offset
                + CH_CARD_ICON_TLUT_REL_OFFSET;
}


bool CH_CardPresentationApply(
    card_file *file,
    s32 sector_size,
    void *sector_buffer,
    u32 presentation_offset,
    const unsigned char *presentation_data,
    u32 presentation_size
)
{
    unsigned char *sector;

    card_stat status;

    u32 required_end;

    s32 result;

    bool payload_matches;
    bool status_matches;
    bool changed = false;

    if (!file
        || !sector_buffer
        || !presentation_data
        || sector_size <= 0)
    {
        return false;
    }

    /*
     * CARD DMA buffers must be 32-byte aligned.
     */
    if (
        ((uintptr_t)sector_buffer & 31u)
        != 0u
    )
    {
        return false;
    }

    /*
     * CarryHandle's current host encoder emits exactly one CI8 banner,
     * one CI8 icon, two RGB5A3 palettes and two comment records.
     */
    if (
        presentation_size
        != CH_CARD_PRESENTATION_CI8_SIZE
    )
    {
        return false;
    }

    /*
     * libogc2 requires icon_addr to begin within the first CARD read
     * block. The value is actually the base of the complete
     * presentation image area, not merely the icon.
     */
    if (
        presentation_offset
        >= CARD_READSIZE
    )
    {
        return false;
    }

    required_end =
        presentation_offset
        + presentation_size;

    if (
        required_end
        < presentation_offset
        || required_end
            > (u32)sector_size
    )
    {
        return false;
    }

    sector =
        (unsigned char *)sector_buffer;

    /*
     * Preserve the application's existing sector-zero contents.
     */
    result =
        CH_MemCardRead(
            file,
            sector,
            (u32)sector_size,
            0
        );

    if (result != CARD_ERROR_READY)
    {
        return false;
    }

    payload_matches =
        memcmp(
            sector
                + presentation_offset,
            presentation_data,
            presentation_size
        ) == 0;

    memset(
        &status,
        0,
        sizeof(status)
    );

    result =
        CARD_GetStatus(
            file->chn,
            file->filenum,
            &status
        );

    if (result != CARD_ERROR_READY)
    {
        return false;
    }

    status_matches =
        presentationStatusMatches(
            &status,
            presentation_offset
        );

    if (!payload_matches)
    {
        memcpy(
            sector
                + presentation_offset,
            presentation_data,
            presentation_size
        );

        result =
            CH_MemCardWrite(
                file,
                sector,
                (u32)sector_size,
                0
            );

        if (result != CARD_ERROR_READY)
        {
            return false;
        }

        changed = true;
    }

    if (!status_matches)
    {
        /*
         * Set packed fields directly rather than using the installed
         * CARD_SetIconSpeed() convenience macro.
         *
         * CarryHandle's current generated format contains exactly one
         * CI8 icon frame at slow speed.
         */
        status.banner_fmt =
            (u8)CARD_BANNER_CI;

        status.icon_addr =
            presentation_offset;

        status.icon_fmt =
            (u16)CARD_ICON_CI;

        status.icon_speed =
            (u16)CARD_SPEED_SLOW;

        status.comment_addr =
            presentation_offset
            + CH_CARD_COMMENT_REL_OFFSET;

        result =
            CARD_SetStatus(
                file->chn,
                file->filenum,
                &status
            );

        if (result != CARD_ERROR_READY)
        {
            return false;
        }

        changed = true;
    }

    /*
     * CARD_GetStatus() also asks libogc2 to derive the physical banner
     * and icon offsets from the metadata we just installed. Verify
     * those calculations rather than merely trusting the write.
     */
    memset(
        &status,
        0,
        sizeof(status)
    );

    result =
        CARD_GetStatus(
            file->chn,
            file->filenum,
            &status
        );

    if (result != CARD_ERROR_READY)
    {
        return false;
    }

    if (
        !presentationStatusMatches(
            &status,
            presentation_offset
        )
    )
    {
        return false;
    }

    if (
        !presentationDerivedOffsetsMatch(
            &status,
            presentation_offset
        )
    )
    {
        return false;
    }

    if (changed)
    {
        /*
         * Verify the actual sector-zero payload after either metadata
         * or data changed.
         */
        result =
            CH_MemCardRead(
                file,
                sector,
                (u32)sector_size,
                0
            );

        if (result != CARD_ERROR_READY)
        {
            return false;
        }

        if (
            memcmp(
                sector
                    + presentation_offset,
                presentation_data,
                presentation_size
            ) != 0
        )
        {
            return false;
        }
    }

    return true;
}
