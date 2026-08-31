#include <stdio.h>

#include <gccore.h>

#include <carryhandle/carryhandle.h>

static void *xfb;
static GXRModeObj *rmode;

static void initialise_console(void)
{
    VIDEO_Init();
    PAD_Init();

    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = SYS_AllocateFramebuffer(rmode);

    CON_Init(
        xfb,
        0,
        0,
        rmode->fbWidth,
        rmode->xfbHeight,
        rmode->fbWidth * VI_DISPLAY_PIX_SZ
    );

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitForFlush();
}

int main(void)
{
    const CH_ApplicationInfo *app;

    initialise_console();

    app = CH_ApplicationGetInfo();

    printf("\x1b[2;0H");

    printf("CarryHandle %s\n", CH_VersionString());
    printf("=====================\n\n");

    printf("Application : %s\n", app->name);
    printf("Game code   : %s\n", app->game_code);
    printf("Company     : %s\n", app->company_code);
    printf(
        "Region      : %s (%d)\n",
        CH_ApplicationRegionName(app->region),
        (int)app->region
    );
    printf("Store ID    : %s\n\n", app->store_id);

    printf("Hello from Nintendo GameCube.\n\n");

    printf("A     : print a controller message\n");
    printf("START : exit\n\n");

    printf(
        "This example deliberately uses libogc2 directly for\n"
        "video and controller setup. CarryHandle does not own\n"
        "those facilities until a reusable CH_* module exists.\n\n"
    );

    while (SYS_MainLoop())
    {
        VIDEO_WaitVSync();
        PAD_ScanPads();

        u32 buttons = PAD_ButtonsDown(0);

        if (buttons & PAD_BUTTON_A)
        {
            printf("GameCube controller A pressed.\n");
        }

        if (buttons & PAD_BUTTON_START)
        {
            printf("START pressed. Goodbye.\n");
            break;
        }
    }

    return 0;
}
