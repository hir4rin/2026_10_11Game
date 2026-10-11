#include "DxLib.h"
#include "Game.h"

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
	SetOutApplicationLogValidFlag(FALSE);
	ChangeWindowMode(TRUE);
	SetGraphMode(kScreenW, kScreenH, 32);
	SetMainWindowText("スマッシュローグ");
	SetWaitVSyncFlag(FALSE); // 60fps 固定は自前で行う
	SetZBufferBitDepth(24);  // 床と影のちらつきを防ぐ（既定の 16bit では足りない）
	if (DxLib_Init() == -1) return -1;
	SetDrawScreen(DX_SCREEN_BACK);

	{
		Game game;
		constexpr LONGLONG kFrameUs = 1000000 / 60;
		LONGLONG next = GetNowHiPerformanceCount();

		while (ProcessMessage() == 0 && !game.IsQuit()) {
			game.Update();
			ClearDrawScreen();
			game.Draw();
			ScreenFlip();

			next += kFrameUs;
			LONGLONG now = GetNowHiPerformanceCount();
			if (now > next + kFrameUs * 5) next = now; // 大きく遅れたら追いつこうとしない
			while (GetNowHiPerformanceCount() < next) {
				if (next - GetNowHiPerformanceCount() > 2000) WaitTimer(1);
			}
		}
	}

	DxLib_End();
	return 0;
}
