#include "../h/slab.h"
#include "../h/Strukture.h"
#include "../h/lockic.h"
#include <math.h>

void buddy_inic()
{
	buddy->mutex = create_mutex();
	uintptr_t adress = buddy->pocetna_adesa;
	unsigned index;
	unsigned nedodeljeni_blokovi = buddy->broj_blokova;
	while (nedodeljeni_blokovi)
	{
		index = (unsigned)floor(log2((double)nedodeljeni_blokovi));
		Buddy_block* bud = adress;
		buddy->niz_slobodnih_blokova[index] = adress;
		bud->next = NULL;
		bud->local = 1 << index;
		adress += (pow(2, index) * BLOCK_SIZE);
		nedodeljeni_blokovi -= (unsigned)pow(2, index);
	}
}

unsigned bajtove_u_min_blokova(unsigned broj_bajtova)
{
	return (unsigned)ceil(1. * broj_bajtova / BLOCK_SIZE);
}

unsigned min_stepen_za_broj_blokova(unsigned broj_blokova)
{
	return (unsigned)ceil(log2((double)broj_blokova));
}

unsigned podeli_blok(unsigned index)
{
	Buddy_block* prvi_buddy = buddy->niz_slobodnih_blokova[index]; // uzmi prvi koji se deli
	buddy->niz_slobodnih_blokova[index] = prvi_buddy->next;     // izbaci onog kog delim

	Buddy_block* next = buddy->niz_slobodnih_blokova[index - 1];
	buddy->niz_slobodnih_blokova[index - 1] = prvi_buddy;

	Buddy_block* drugi_buddy = (Buddy_block*)((uintptr_t)prvi_buddy + (unsigned)(pow(2, index - 1) * BLOCK_SIZE));
	drugi_buddy->next = NULL;
	drugi_buddy->local = prvi_buddy->local | (1 << index - 1);
	prvi_buddy->next = drugi_buddy;

	return index - 1;
}

void premesti_slab(Slab_block* slab_block, unsigned iz_praznog_slaba)
{
	Kes* kes = slab_block->header.moj_kes;
	/*if (!kes->prazan && iz_praznog_slaba)
		iz_praznog_slaba = 0;*/

	if (iz_praznog_slaba)
	{ // potice iz praznog
		Slab_block* prazan = kes->prazan;
		Slab_block* prev_prazan = NULL;
		while (prazan) {
			if (prazan == slab_block)
				break;
			prev_prazan = prazan;
			prazan = prazan->header.sledeci;
		}
		if (!prev_prazan) {
			if (!kes->prazan)
				printf("tu is");
			kes->prazan = kes->prazan->header.sledeci;
		}
		else
			prev_prazan->header.sledeci = prazan->header.sledeci;
	}
	else
	{ // potice iz nepunog
		if (slab_block->header.broj_slobodnih_slotova)
			return;
		Slab_block* nepun = kes->nepun;
		Slab_block* prev_nepun = NULL;
		while (nepun) {
			if (nepun == slab_block)
				break;
			prev_nepun = nepun;
			nepun = nepun->header.sledeci;
		}
		if (!prev_nepun)
			kes->nepun = kes->nepun->header.sledeci;
		else
			prev_nepun->header.sledeci = nepun->header.sledeci;

	}
	if (slab_block->header.broj_slobodnih_slotova)
	{ // staviti ga u nepune
		Slab_block* nepun = kes->nepun;
		Slab_block* prev_nepun = NULL;

		while (nepun) {
			if (slab_block->header.broj_slobodnih_slotova > nepun->header.broj_slobodnih_slotova)
				break;
			prev_nepun = nepun;
			nepun = nepun->header.sledeci;
		}
		if (!nepun)
			kes->nepun = slab_block;
		else if (!prev_nepun)
		{ //slab_block->header.sledeci = 
			kes->nepun->header.sledeci = slab_block;
			return;
		}
		else {
			slab_block->header.sledeci = prev_nepun->header.sledeci;
			prev_nepun->header.sledeci = slab_block;
			return;
		}
	}
	else
	{ // staviti ga u pune
		Slab_block* pun = kes->pun;
		Slab_block* prev_pun = NULL;

		if (kes->pun)
			slab_block->header.sledeci = kes->pun;
		kes->pun = slab_block;
	}
}

void* slot_alloc(Slab_block* slab_block, unsigned* iz_praznog_slaba)
{
	unsigned index = index_slobodnog_slota(slab_block->header.niz_slobodnih_slotova, slab_block->header.broj_slotova);
	if (index == -1)
		return NULL;
	slab_block->header.broj_slobodnih_slotova--;
	premesti_slab(slab_block, *iz_praznog_slaba);
	return slab_block->header.prvi_slot + index * slab_block->header.velicina_slota;

	/*uintptr_t prazan_slot = slab_block->header.prvi_slot;

	for (size_t i = 0; i < slab_block->header.broj_slotova; i++)
	{
		if (*(char*)prazan_slot == 0)
		{
			slab_block->header.broj_slobodnih_slotova--;
			premesti_slab(slab_block, *iz_praznog_slaba);
			return prazan_slot;
		}
		prazan_slot += slab_block->header.velicina_slota;
	}
	return NULL;*/
}

void slab_info(Slab_block* slab_block) {
	if (!slab_block)
		printf("\tNe postoje\n");

	Slab_block* next = slab_block;
	while (next) {
		//printf("\tAdresa slaba : %p\n", next);
		printf("\tBroj blokova: %d\n", (unsigned)pow(2, next->header.stepen_dvojke));
		//printf("\tAdresa Prvog slota: %zd\n", next->header.prvi_slot);
		printf("\tVelicina slota : %d\n", next->header.velicina_slota);
		printf("\tBroj slotova : %d\n", next->header.broj_slotova);
		printf("\tBroj slobodnih slotova : %d\n", next->header.broj_slobodnih_slotova);
		//printf("\tAdresa sledeceg slaba : %p\n", next->header.sledeci);
		printf("\tPopunjenost : %f \n", next->header.broj_slobodnih_slotova * 1. / next->header.broj_slotova);
		next = next->header.sledeci;
	}
}

Slab_block* slab_alloc_typed(Kes* moj_kes)
{
	unsigned potrebno_blokova = bajtove_u_min_blokova(moj_kes->velicina + sizeof(Slab_block_header));
	unsigned min_stepen = min_stepen_za_broj_blokova(potrebno_blokova);

	unsigned sledeci_stepen = min_stepen;
	while (sledeci_stepen < VELICINA_PAMTLJIVOG) {
		if (buddy->niz_slobodnih_blokova[sledeci_stepen])
			break;
		sledeci_stepen++;
	}
	if (sledeci_stepen == VELICINA_PAMTLJIVOG)
		return NULL; // nema slobodnih
	while (sledeci_stepen < VELICINA_PAMTLJIVOG) // radi deobu
	{
		if (sledeci_stepen == min_stepen)
		{
			Buddy_block* slobodan = buddy->niz_slobodnih_blokova[min_stepen];
			unsigned short local = slobodan->local;
			buddy->niz_slobodnih_blokova[min_stepen] = slobodan->next;
			slobodan->next = NULL;

			Slab_block* ret = (Slab_block*)slobodan;
			ret->header.moj_kes = moj_kes;
			ret->header.stepen_dvojke = min_stepen;
			ret->header.sledeci = NULL;
			ret->header.local = local;
			ret->header.velicina_slota = moj_kes->velicina;
			unsigned pomeraj = 0;
			ret->header.broj_slobodnih_slotova = ret->header.broj_slotova =
				broj_slotova(ret->header.velicina_slota, ret->header.stepen_dvojke, &pomeraj);
			ret->header.niz_slobodnih_slotova = (uintptr_t)ret + sizeof(Slab_block_header);
			if (pomeraj >= CACHE_L1_LINE_SIZE) {
				if (!moj_kes->pomeraj) {
					moj_kes->pomeraj = pomeraj;
					pomeraj = 0;
				}
				else {
					unsigned old = moj_kes->pomeraj;
					moj_kes->pomeraj -= CACHE_L1_LINE_SIZE;
					pomeraj = CACHE_L1_LINE_SIZE;
					if (moj_kes->pomeraj < CACHE_L1_LINE_SIZE)
						moj_kes->pomeraj = 0;
				}

			}
			else pomeraj = 0;
			ret->header.prvi_slot = (uintptr_t)ret + sizeof(Slab_block_header) +
				ceil(ret->header.broj_slotova / 8.) + pomeraj;

			unsigned velicina_niza = ceil(ret->header.broj_slotova / 8.);
			memset(ret->header.niz_slobodnih_slotova, 0xFF, ret->header.broj_slotova);
			unsigned set = pow(2, ret->header.stepen_dvojke) * BLOCK_SIZE - sizeof(Slab_block_header) - velicina_niza;
			memset((uintptr_t)ret + sizeof(Slab_block_header) + velicina_niza, 0, set);

			return ret;
		}
		sledeci_stepen = podeli_blok(sledeci_stepen);
	}
	return NULL;
}

Slab_block* slab_alloc_buffered(Kes* moj_kes)
{
	unsigned potrebno_blokova = bajtove_u_min_blokova(moj_kes->velicina);
	unsigned min_stepen = min_stepen_za_broj_blokova(potrebno_blokova);

	unsigned sledeci_stepen = min_stepen;
	while (sledeci_stepen < VELICINA_PAMTLJIVOG) {
		if (buddy->niz_slobodnih_blokova[sledeci_stepen])
			break;
		sledeci_stepen++;
	}
	if (sledeci_stepen == VELICINA_PAMTLJIVOG)
		return NULL; // nema slobodnih

	while (sledeci_stepen < VELICINA_PAMTLJIVOG) // radi deobu
	{
		if (sledeci_stepen == min_stepen)
		{
			Buddy_block* slobodan = buddy->niz_slobodnih_blokova[min_stepen];
			unsigned short local = slobodan->local;
			buddy->niz_slobodnih_blokova[min_stepen] = slobodan->next;
			slobodan->next = NULL;

			Slab_block* ret = (Slab_block*)slobodan;
			ret->header.moj_kes = moj_kes;
			ret->header.stepen_dvojke = min_stepen;
			ret->header.sledeci = NULL;
			ret->header.local = local;
			ret->header.velicina_slota = moj_kes->velicina;
			ret->header.broj_slobodnih_slotova = ret->header.broj_slotova = 
				broj_slotova(ret->header.velicina_slota, ret->header.stepen_dvojke, NULL);
			ret->header.niz_slobodnih_slotova = (uintptr_t)ret + sizeof(Slab_block_header);
			ret->header.prvi_slot = (uintptr_t)ret + sizeof(Slab_block_header) +
				ceil(ret->header.broj_slotova / 8.);

			unsigned velicina_niza = ceil(ret->header.broj_slotova / 8.);
			memset(ret->header.niz_slobodnih_slotova, 0xFF, ret->header.broj_slotova);
			unsigned set = pow(2, ret->header.stepen_dvojke) * BLOCK_SIZE - sizeof(Slab_block_header) - velicina_niza;
			memset((uintptr_t)ret + sizeof(Slab_block_header) + velicina_niza, 0, set);

			return ret;
		}
		sledeci_stepen = podeli_blok(sledeci_stepen);
	}
	return NULL;

}

Buddy_block* spoji_ako_je_brat_slobodan(Buddy_block* buddy_brat, size_t* stepen_dvojke)
{
	//memset((uintptr_t)buddy_brat + sizeof(Buddy_block), 0, pow(2, *stepen_dvojke) - sizeof(Buddy_block));
	Buddy_block* next = buddy->niz_slobodnih_blokova[*stepen_dvojke];
	if (!next) {
		buddy->niz_slobodnih_blokova[*stepen_dvojke] = buddy_brat;
		return NULL;
	}
	Buddy_block* prethodni = NULL;
	while (next) {
		unsigned short mask = 1 << *stepen_dvojke;
		mask &= buddy_brat->local;
		mask >>= *stepen_dvojke;
		if (!mask)
			mask = buddy_brat->local + pow(2, *stepen_dvojke);
		else
			mask = buddy_brat->local - pow(2, *stepen_dvojke);

		if (next->local == mask)
			break;
		prethodni = next;
		next = next->next;
	}
	if (!next)
		return NULL;
	if (prethodni) {
		prethodni->next = next->next;
	}
	else
		buddy->niz_slobodnih_blokova[*stepen_dvojke] = next->next;
	(*stepen_dvojke)++;
	if ((uintptr_t)buddy_brat < (uintptr_t)next)
		return buddy_brat;
	else
		return next;
}

unsigned oslobodi(Buddy_block* buddy_block, size_t stepen_dvojke)
{
	Buddy_block* next = spoji_ako_je_brat_slobodan(buddy_block, &stepen_dvojke);
	while (next)
	{
		next = spoji_ako_je_brat_slobodan(next, &stepen_dvojke);
	}
	return (unsigned)stepen_dvojke;
}

int index_slobodnog_slota(char* niz_slobodnih_slotova, unsigned broj_slotova) {
	int index = 0;
	char mask = 1;
	char* start = niz_slobodnih_slotova - 1;
	while (index < broj_slotova) {
		if (index % 8 == 0) {
			mask = 1;
			start++;
		}
		if (*start & mask) {
			int reset = pow(2, mask - 1);
			*start -= reset;
			return index;
		}
		index++;
		mask <<= 1;
	}
	return -1;
}

void oslodi_slot(char* niz_slobodnih_slotova, unsigned index) {
	char mask = 1;
	char shift = index % 8;
	mask <<= shift;

	char* start = niz_slobodnih_slotova + index;
	*start |= mask;
}

unsigned broj_slotova(unsigned velicina_slota, unsigned stepen_dvojke, unsigned* fragment) {
	unsigned preostali_bajtovi = BLOCK_SIZE * pow(2, stepen_dvojke) - sizeof(Slab_block_header);
	unsigned broj_slotova = floor(preostali_bajtovi / (velicina_slota + 0.125));
	if(fragment)
		*fragment = preostali_bajtovi - broj_slotova * velicina_slota;
	return broj_slotova;
}

unsigned neiskorisceno_bajtova(Slab_block* slab_block) {
	return  pow(2, slab_block->header.stepen_dvojke) * BLOCK_SIZE -
		sizeof(Slab_block_header) -
		slab_block->header.broj_slotova * (slab_block->header.velicina_slota + 1. / 8);
}


