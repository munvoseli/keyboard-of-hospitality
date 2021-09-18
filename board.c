// gcc `pkg-config --cflags sdl2` board.c `pkg-config --libs sdl2` -lm

#include <SDL.h>
#include <SDL_audio.h>
#include <math.h>

#define SAMPLE_RATE 48000
#define AMPLITUDE 14000
#define MAX_OSCILLATORS 10
#define C_STACK 10
#define C_NBUFF 10

// struct for a single oscillator	
struct Osc
{
	double volume;
	double targetVolume;
	int note; // pitch; 0 = A4 / 440 Hz
	unsigned int samplesSinceHit;
	double nSampleProgress; // progress in wavelength
	double cSampleWavelength; // wavelength
};

// struct for data for all the oscillators, and maybe some other data too
struct Oscset
{
	unsigned int instrument;
	double focusedDecrease; // how fast the active note decreases in volume
	double unfocusedDecrease;
	unsigned int cOscillator; // count of active oscillators
	int currentNote; // this was for keeping track of the focused note.  don't use it
	int lastPlayedNote; // because silent notes for moving
	unsigned int nSelectedStack;
	int oscStack [C_STACK]; // not a stack? scrolling, not push/pop
	unsigned int nBufferedNotes;
	char bufferingNote;
	int oscBuffer [C_NBUFF];
	struct Osc aOsc [MAX_OSCILLATORS]; // array of oscillators
};

void init_oscset (struct Oscset * idatap)
{
	int i;
	idatap->instrument = 0;
	idatap->focusedDecrease = 3;
	idatap->cOscillator = 2;
	idatap->currentNote = 0;
	idatap->lastPlayedNote = 0;
	
	idatap->nSelectedStack = 0;
	for (i = 0; i < C_STACK; ++i)
		idatap->oscStack [i] = 0;
	
	idatap->nBufferedNotes = 0;
	idatap->bufferingNote = 0;
	for (i = 0; i < MAX_OSCILLATORS; ++i)
		idatap->aOsc [i].samplesSinceHit = 0;
}

void setnote (struct Osc * oscp, int note)
{
	oscp->note = note;
	// freq = 440 * pow (2, note / 12) cycle / sec
	// wavelength = 44100 sample/sec / (440 * pow(..) cycle/sec) == sample/cycle
	oscp->cSampleWavelength = SAMPLE_RATE / (440 * pow (2, note / (double) 12));
}

void addnotetostack (struct Oscset * idatap)
{
	// do not add to stack if note (same pitch) is already on the stack
	unsigned int i;
	for (i = 0; i < C_STACK; ++i)
		if (idatap->oscStack [i] == idatap->currentNote)
			return;
	idatap->oscStack [idatap->nSelectedStack] = idatap->currentNote;
	++idatap->nSelectedStack;
	if (idatap->nSelectedStack >= C_STACK)
		idatap->nSelectedStack = 0;
}

// hit note
void addnotetoset (struct Oscset * idatap)
{
	int i;
	char makeNew = 1;
	idatap->lastPlayedNote = idatap->currentNote;
	if (idatap->bufferingNote)
	{
		idatap->oscBuffer [idatap->nBufferedNotes] = idatap->lastPlayedNote;
		addnotetostack (idatap);
		idatap->bufferingNote = 0;
		++idatap->nBufferedNotes;
		return;
	}
	for (i = 0; i < idatap->cOscillator; ++i)
	{
		if (idatap->aOsc [i].note == idatap->currentNote)
		{
			makeNew = 0;
			idatap->aOsc [i].targetVolume += 1;
			//idatap->aOsc [i].samplesSinceHit = 0;
			if (idatap->aOsc [i].targetVolume > 1)
				idatap->aOsc [i].targetVolume = 1;
			break;
		}
	}
	if (makeNew)
	{
		idatap->aOsc [idatap->cOscillator].volume = 0;
		idatap->aOsc [idatap->cOscillator].targetVolume = 1;
		idatap->aOsc [idatap->cOscillator].samplesSinceHit = 0;
		setnote (&idatap->aOsc [idatap->cOscillator], idatap->currentNote);
		++idatap->cOscillator;
	}
	addnotetostack (idatap);
	if (idatap->nBufferedNotes > 0)
	{
		--idatap->nBufferedNotes;
		idatap->currentNote = idatap->oscBuffer [idatap->nBufferedNotes];
		addnotetoset (idatap);
	}
}

void playfromstack (struct Oscset * idatap, unsigned int n_stack)
{
	idatap->currentNote = idatap->oscStack [n_stack];
	addnotetoset (idatap);
}

int event_handler (SDL_Event * eventp, struct Oscset * idatap)
{
	int i;
	if (eventp->type == SDL_QUIT)
	{
		//for (i = 0; i < MAX_OSCILLATORS; ++i)
		//	free (idatap->aOsc [i]);
		SDL_PauseAudio (0);
		SDL_CloseAudio ();
		SDL_Quit ();
		return 0; // exit
	}
	else if (eventp->type == SDL_KEYDOWN)
	{
		switch (eventp->key.keysym.sym)
		{
		case SDLK_z: idatap->currentNote -= 5;
		case SDLK_x: --idatap->currentNote;
		case SDLK_c: --idatap->currentNote;
		case SDLK_v: --idatap->currentNote;
		case SDLK_a: --idatap->currentNote;
		case SDLK_s: --idatap->currentNote;
		case SDLK_d: --idatap->currentNote;
		case SDLK_f: --idatap->currentNote;
			addnotetoset (idatap);
			break;
		case SDLK_SLASH: idatap->currentNote += 5;
		case SDLK_PERIOD: ++idatap->currentNote;
		case SDLK_COMMA: ++idatap->currentNote;
		case SDLK_m: ++idatap->currentNote;
		case SDLK_SEMICOLON: ++idatap->currentNote;
		case SDLK_l: ++idatap->currentNote;
		case SDLK_k: ++idatap->currentNote;
		case SDLK_j: ++idatap->currentNote;
		case SDLK_h:
			addnotetoset (idatap);
			break;
		case SDLK_g:
			idatap->currentNote = 0;
			addnotetoset (idatap);
			break;
		case SDLK_p: idatap->currentNote += 12; break;
		case SDLK_q: idatap->currentNote -= 12; break;
		case SDLK_0: playfromstack (idatap, 0); break;
		case SDLK_1: playfromstack (idatap, 1); break;
		case SDLK_2: playfromstack (idatap, 2); break;
		case SDLK_3: playfromstack (idatap, 3); break;
		case SDLK_4: playfromstack (idatap, 4); break;
		case SDLK_5: playfromstack (idatap, 5); break;
		case SDLK_6: playfromstack (idatap, 6); break;
		case SDLK_7: playfromstack (idatap, 7); break;
		case SDLK_8: playfromstack (idatap, 8); break;
		case SDLK_9: playfromstack (idatap, 9); break;
		case SDLK_w: (idatap->focusedDecrease -= (double) .5) < 0 ? idatap->focusedDecrease = 0:0; break;
		case SDLK_e: idatap->focusedDecrease += .5; break;
		case SDLK_u: idatap->bufferingNote = 1;
		case SDLK_t: idatap->instrument ^= 1;
		}
		return 1;
	}
	return 1;
}


static inline Sint16 instrument_hotel (int j, struct Oscset * idatap)
{
	// first half: saw from 0 to 1/2 (-1 to 0), slope 2
	// second half: saw from 1/4 to 1 (-1/2 to 1), slope 3
	if (2 * idatap->aOsc [j].nSampleProgress < idatap->aOsc [j].cSampleWavelength)
		return AMPLITUDE * idatap->aOsc [j].volume * (2 * idatap->aOsc [j].nSampleProgress / idatap->aOsc [j].cSampleWavelength - (double) .75);
	else
		return AMPLITUDE * idatap->aOsc [j].volume * (2 * idatap->aOsc [j].nSampleProgress / idatap->aOsc [j].cSampleWavelength - (double) 1);
}

static inline Sint16 instrument_triangle (int j, struct Oscset * idatap)
{
	if (2 * idatap->aOsc [j].nSampleProgress < idatap->aOsc [j].cSampleWavelength)
		return AMPLITUDE * (2 * idatap->aOsc [j].nSampleProgress / idatap->aOsc [j].cSampleWavelength - (double) .5);
	else
		return AMPLITUDE * (-2 * idatap->aOsc [j].nSampleProgress / idatap->aOsc [j].cSampleWavelength + (double) 1.5);
}

double lerp
(double x, double y, double a)
{
	return x + a * (y - x);
}

void audio_callback (void * datap, Uint8 * raw_buffer, int cbyte)
{
	Sint16 * buffer;
	int length;
	struct Oscset * idatap;
	int i, j;
	double time;
	SDL_Event event;
	Sint16 diff;
	length = cbyte / 2;
	buffer = (Sint16*) raw_buffer;
	idatap = (struct Oscset*) datap;
	Sint16 (*p_instrument) (int, struct Oscset*) = idatap->instrument == 0 ? &instrument_hotel : &instrument_triangle;
	for (i = 0; i < length; ++i)
	{
		buffer [i] = 0;
		for (j = 0; j < idatap->cOscillator; ++j)
		{
			//buffer [i] += (Sint16) (AMPLITUDE * idatap->aOsc [j]->volume * (idatap->aOsc [j]->nSampleProgress / idatap->aOsc [j]->cSampleWavelength - (double) .5));

			// second instrument: accidental hospitality
			

			// third instrument: triangle wave
			diff = (*p_instrument) (j, idatap) * lerp (idatap->aOsc [j].volume, idatap->aOsc [j].targetVolume, (double) i / (double) length);
			
			diff *= (sin ((double) idatap->aOsc [j].samplesSinceHit / (double) 1000) + (double) 5) / (double) 5;
			buffer [i] += diff;
			idatap->aOsc [j].nSampleProgress = fmod (idatap->aOsc [j].nSampleProgress + 1, idatap->aOsc [j].cSampleWavelength);
			++idatap->aOsc [j].samplesSinceHit;
		}
	}
	for (j = 0; j < idatap->cOscillator; ++j)
	{
		// more samples / call -> more volume decrease / call
		// more samples / sec -> less volume decrease / sample
		// decrese x / call * call / sec * sec / SAMPLE_RATE samples
		time = idatap->focusedDecrease;//idatap->lastPlayedNote == idatap->aOsc [j].note ? idatap->focusedDecrease : idatap->unfocusedDecrease;
		idatap->aOsc [j].volume = idatap->aOsc [j].targetVolume;
		idatap->aOsc [j].targetVolume = idatap->aOsc [j].volume - time * (double) length / SAMPLE_RATE;
		if (idatap->aOsc [j].volume <= 0)
		{
			idatap->aOsc [j] = idatap->aOsc [idatap->cOscillator - 1];
			--j;
			--idatap->cOscillator;
		}
		if (idatap->aOsc [j].targetVolume < 0)
			idatap->aOsc [j].targetVolume = 0;
	}
}

int main ()
{
	SDL_AudioSpec want;
	SDL_AudioSpec have;
	int i;
	struct Oscset data;
	int keep_running;
	SDL_Event event;
	SDL_Window * screen = SDL_CreateWindow (
		"hi",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		100, 100, 0);
	init_oscset (&data);
	if (SDL_Init (SDL_INIT_AUDIO) != 0)
		printf ("SDL init failed: %s", SDL_GetError ());
	want.freq = SAMPLE_RATE;
	want.format = AUDIO_S16SYS;
	want.channels = 1;
	want.samples = 2048;
	want.callback = audio_callback;
	want.userdata = &data;
	if (SDL_OpenAudio (&want, &have) != 0)
		printf ("SDL failed open audio: %s", SDL_GetError ());
	if (want.format != have.format)
		printf ("Failed to get desired AudioSpec");
	SDL_PauseAudio (0);
	keep_running = 1;
	while (keep_running)
	{
		while (SDL_PollEvent (&event))
			keep_running &= event_handler (&event, &data);
		SDL_Delay (10);
	}
	return 0;
}
