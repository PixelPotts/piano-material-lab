/* Minimal Windows waveform API declarations for the bundled C compiler. */
typedef HANDLE HWAVEOUT;
typedef UINT MMRESULT;
#pragma pack(push,1)
typedef struct {
    WORD wFormatTag, nChannels;
    DWORD nSamplesPerSec, nAvgBytesPerSec;
    WORD nBlockAlign, wBitsPerSample, cbSize;
} WAVEFORMATEX;
#pragma pack(pop)
typedef struct wavehdr_tag {
    LPSTR lpData;
    DWORD dwBufferLength, dwBytesRecorded;
    DWORD_PTR dwUser;
    DWORD dwFlags, dwLoops;
    struct wavehdr_tag *lpNext;
    DWORD_PTR reserved;
} WAVEHDR;
#define WAVE_FORMAT_PCM 1
#define WAVE_MAPPER ((UINT)-1)
#define CALLBACK_NULL 0
#define CALLBACK_EVENT 0x00050000
#define MMSYSERR_NOERROR 0
#define WHDR_PREPARED 2
#define WHDR_INQUEUE 16
__declspec(dllimport) MMRESULT WINAPI waveOutOpen(HWAVEOUT *,UINT,const WAVEFORMATEX *,DWORD_PTR,DWORD_PTR,DWORD);
__declspec(dllimport) MMRESULT WINAPI waveOutPrepareHeader(HWAVEOUT,WAVEHDR *,UINT);
__declspec(dllimport) MMRESULT WINAPI waveOutUnprepareHeader(HWAVEOUT,WAVEHDR *,UINT);
__declspec(dllimport) MMRESULT WINAPI waveOutWrite(HWAVEOUT,WAVEHDR *,UINT);
__declspec(dllimport) MMRESULT WINAPI waveOutReset(HWAVEOUT);
__declspec(dllimport) MMRESULT WINAPI waveOutClose(HWAVEOUT);
__declspec(dllimport) MMRESULT WINAPI waveOutPause(HWAVEOUT);
__declspec(dllimport) MMRESULT WINAPI waveOutRestart(HWAVEOUT);
