#include <string.h>
#include "../h/slab.h"
#include "../h/Strukture.h"
#include "../h/lockic.h"

#define BLOCK_SIZE (4096)

int index_tipskog_kesa(Kes* kes)
{
	for (size_t i = 0; i < BROJ_TIPSKIH_KESEVA; i++)
		if (kes == slab->ulancani_kesevi + i) {
			if (slab->niz_napravljenih_keseva[i])
				return i;
			return -1;
		}
	return -1;
}

void inic_baferske_keseve()
{
	char buffer[15];
	for (size_t i = 0; i < 13; i++)
	{
		memset(slab->baferisani_kesevi + i, 0, sizeof(Kes));
		sprintf_s(buffer, 15, "Mem Buffer %d", i + 5);
		slab->baferisani_kesevi[i].naziv = buffer;
		slab->baferisani_kesevi[i].velicina = pow(2, i + 5) - 1 - sizeof(Slab_block_header);
		slab->baferisani_kesevi[i].mutex = create_mutex();
	}
}

void inic_tipske_keseve() {
	memset(slab->ulancani_kesevi, 0, sizeof(Kes) * BROJ_TIPSKIH_KESEVA);
}

void slab_inic(uintptr_t pocetna_adresa, unsigned ukupan_broj_blokova)
{
	slab = (Slab*)pocetna_adresa;
	inic_baferske_keseve();
	slab->mutex = create_mutex();

	slab->ulancani_kesevi = (Kes*)(pocetna_adresa + sizeof(Slab)); // prazna lista keseva
	inic_tipske_keseve();

	memset(slab->niz_napravljenih_keseva, 0, BROJ_TIPSKIH_KESEVA * sizeof(unsigned char));

	slab->buddy.broj_blokova = ukupan_broj_blokova - BROJ_REZERVISANIH_BLOKOVA; // ova linija moze da bude promeljiva

	slab->buddy.pocetna_adesa = pocetna_adresa + BROJ_REZERVISANIH_BLOKOVA * BLOCK_SIZE;

	buddy = &slab->buddy;
	memset(buddy->niz_slobodnih_blokova, 0, VELICINA_PAMTLJIVOG * sizeof(Buddy_block*));
	buddy_inic();
}

Kes* daj_prazno_mesto_za_kes()
{
	for (size_t i = 0; i < BROJ_TIPSKIH_KESEVA; i++)
		if (!slab->niz_napravljenih_keseva[i])
		{
			slab->niz_napravljenih_keseva[i] = 1;
			return slab->ulancani_kesevi + i;
		}
	return NULL;
}

Kes* kes_alloc(const char* naziv, size_t velicina, // size je broj blokova ili velcina objekta tog tog tipa
	void (*ctor)(void*), void (*dtor)(void*))
{
	kmem_cache_t* kes = daj_prazno_mesto_za_kes();
	if (!kes)
		return NULL; // nema mesta vise

	kes->naziv = naziv;
	kes->velicina = velicina;
	kes->ctor = ctor;
	kes->dtor = dtor;
	kes->mutex = create_mutex();

	kes->prazan = slab_alloc_typed(kes);
	if (!kes->prazan)
		return NULL;

	return kes;
}

void oslobodi_slabove_kesa(Kes* kes)
{
	Slab_block* next = kes->nepun;
	Buddy_block* za_oslobadjanje = NULL;
	unsigned short local;
	unsigned short stepen;
	while (next)
	{
		local = next->header.local;
		stepen = next->header.stepen_dvojke;
		za_oslobadjanje = next;
		za_oslobadjanje->local = local;
		next = next->header.sledeci;
		oslobodi(za_oslobadjanje, stepen);
	}

	next = kes->pun;
	while (next)
	{
		local = next->header.local;
		stepen = next->header.stepen_dvojke;
		za_oslobadjanje = next;
		za_oslobadjanje->local = local;
		next = next->header.sledeci;
		oslobodi(za_oslobadjanje, stepen);
	}
	next = kes->prazan;
	while (next)
	{
		local = next->header.local;
		stepen = next->header.stepen_dvojke;
		za_oslobadjanje = next;
		za_oslobadjanje->local = local;
		next = next->header.sledeci;
		oslobodi(za_oslobadjanje, stepen);
	}
	kes->prazan = NULL;
	kes->nepun = NULL;
	kes->pun = NULL;
}

void kes_free(Kes* kes)
{
	int index = index_tipskog_kesa(kes);
	if (index == -1)
		return NULL;
	oslobodi_slabove_kesa(kes);
	slab->niz_napravljenih_keseva[index] = 0;
	memset(slab->ulancani_kesevi + index, 0, sizeof(Kes));
}

Slab_block* obezbedi_slab_za_typed_obj(Kes* kes, unsigned* iz_praznog_slaba)
{
	*iz_praznog_slaba = 0;
	if (!kes->nepun) {
		*iz_praznog_slaba = 1;
		if (kes->prazan) {
			if (kes != kes->prazan->header.moj_kes)
				printf("error");
			return kes->prazan;
		}
		else
			return kes->prazan = slab_alloc_typed(kes);
	}
	return kes->nepun;
}

Slab_block* obezbedi_slab_za_buff_obj(Kes* kes) {
	if (kes->prazan)
		return kes->prazan;
	return kes->prazan = slab_alloc_buffered(kes);
}

void* obj_type_alloc(Kes* kes) // vraca obj tipa koji kes cuva
{
	if (index_tipskog_kesa(kes) == -1)
		return NULL;
	int iz_praznog_slaba = 1;
	Slab_block* slab_block = obezbedi_slab_za_typed_obj(kes, &iz_praznog_slaba);
	if (!slab_block) {
		kes->error = NUSPELA_ALOKACIJA_SLABA;
		return NULL;
	}
	void* slot = slot_alloc(slab_block, &iz_praznog_slaba);
	if (!slot) {
		kes->error = NUSPELA_ALOKACIJA_SLOTA;
		return NULL;
	}
	if (kes->ctor)
		kes->ctor(slot);
	return slot;
}

void* obj_buffer_alloc(Kes* kes) // vraca obj tipa koji kes cuva
{
	Slab_block* slab_block = obezbedi_slab_za_buff_obj(kes);
	if (!slab_block) {
		kes->error = NUSPELA_ALOKACIJA_SLABA;
		return NULL;
	}
	unsigned iz_praznog_slaba = 1;
	void* slot = slot_alloc(slab_block, &iz_praznog_slaba);
	if (!slot) {
		kes->error = NUSPELA_ALOKACIJA_SLOTA;
		return NULL;
	}
	return slot;
}

Slab_block* obrisi_objekt_u_slabu(Slab_block* slab_block, void* obj)
{
	uintptr_t prazan_slot = slab_block->header.prvi_slot;
	for (size_t i = 0; i < slab_block->header.broj_slotova; i++)
	{
		if (obj == (void*)prazan_slot)
		{
			if (slab_block->header.moj_kes->dtor)
				slab_block->header.moj_kes->dtor(obj);
			oslodi_slot(slab_block->header.niz_slobodnih_slotova, i);
			slab_block->header.broj_slobodnih_slotova++;
			return slab_block;
		}
		prazan_slot += slab_block->header.velicina_slota;
	}
	return NULL;
}

Slab_block* pronadji_slab_objekta_kog_brises(Kes* kes, void* obj)
{
	Slab_block* ret = NULL;
	Slab_block* prev = NULL;
	Slab_block* next = kes->pun;
	while (next)
	{ // pretrazuje pune
		if (ret = obrisi_objekt_u_slabu(next, obj))
		{ // nasao slab objekta kojeg izbacuje
			if (prev) // pun slab ima prethodnika u listi
				prev->header.sledeci = ret->header.sledeci; // izbacen iz liste punih
			else // pun slab nema prethnodnika u listi
				kes->pun = ret->header.sledeci; // izbacen iz liste punih
			if (ret->header.broj_slobodnih_slotova == ret->header.broj_slotova)
			{ // prazan je, treba ga staviti u prazne slabove
				if (kes->prazan)
					ret->header.sledeci = kes->prazan;
				kes->prazan = ret;
				return ret;
			}
			else
			{ // nije prazan, treba ga ubaciti po sortu u nepune
				prev = NULL;
				if (!kes->nepun)
					kes->nepun = ret;
				else {
					next = kes->nepun;
					while (next) {
						if (ret->header.broj_slobodnih_slotova > next->header.broj_slobodnih_slotova)
							break;
						prev = next;
						next = next->header.sledeci;
					}
					ret->header.sledeci = next;
					prev->header.sledeci = ret;

				}
				return ret;
			}
		}
		prev = next;
		next = next->header.sledeci;
	}


	next = kes->nepun;
	prev = NULL;
	while (next)
	{ // pretrazuje nepune
		if (ret = obrisi_objekt_u_slabu(next, obj))
			// nasao slab objekta kojeg izbacuje
			if (ret->header.broj_slobodnih_slotova == ret->header.broj_slotova)
			{ // prazan je, treba ga staviti u prazne slabove
				if (!prev)
				{ // nema prethondika u nepunim
					kes->nepun = kes->nepun->header.sledeci;

					if (!kes->prazan) // nema ni jedan prazan slab
						kes->prazan = ret;
					else
					{ // ima makar jedan prazan slab
						ret = kes->prazan->header.sledeci;
						kes->prazan->header.sledeci = ret;
					}
				}
				return ret;
			}
			else
			{ // nije prazan, treba proveriti da li nepuni ostaju sortirani
				if (prev) {
					if (prev->header.broj_slobodnih_slotova < ret->header.broj_slobodnih_slotova)
					{ // mora da se sortira, prethodni ima vise zauzetih
						next = kes->nepun;
						Slab_block* t = ret->header.sledeci;
						if (prev = next)
						{ // prev nema prethodnika
							ret->header.sledeci = prev;
							prev->header.sledeci = t;
							kes->nepun = ret;
						}
						else
						{ // prev ima prethodnike
							while (next->header.sledeci != prev)
								next = next->header.sledeci;
							prev->header.sledeci = t;
							ret->header.sledeci = prev;
							next->header.sledeci = ret;
						}
					}
				}
				return ret;
			}
		prev = next;
		next = next->header.sledeci;
	}
	return NULL;
}

void obj_free(Kes* kes, void* obj)
{
	int index = index_tipskog_kesa(kes);
	if (index == -1)
		return;
	Slab_block* za_brisanje = pronadji_slab_objekta_kog_brises(kes, obj);
	if (!za_brisanje)
		kes->error = OBJEKAT_ZA_KOJE_JE_ZATEVANO_BRISANJE_SE_NE_NALAZI_U_KESU;
}

void* buff_alloc(size_t size)
{
	unsigned stepen = min_stepen_za_broj_blokova(size);
	// kontrola da li moze da stane u taj kes
	if (stepen < 5 || stepen > 17)
		return NULL; // ne znam kome se salje ovaj error
	return obj_buffer_alloc(&slab->baferisani_kesevi[stepen - 5]);
}

void buff_free(const void* buff)
{
	Kes* kes;
	for (size_t i = 0; i < BROJ_KESEVA_ZA_BAFERE; i++) {
		if (kes = pronadji_slab_objekta_kog_brises(&slab->baferisani_kesevi[i], buff)) {
			return;
		}
	}
	kes->error = OBJEKAT_ZA_KOJE_JE_ZATEVANO_BRISANJE_SE_NE_NALAZI_U_KESU;
}

int skupi_kes(Kes* kes) // vraca broj oslobodjenih blokova
{
	int index = index_tipskog_kesa(kes);
	if (index == -1)
		return NULL;
	int osobodjeni_blokovi = 0;
	Slab_block* next = kes->prazan;
	Buddy_block* buddy;
	unsigned stepen;
	unsigned short local;
	while (next) {
		kes->prazan = next->header.sledeci;
		stepen = next->header.stepen_dvojke;
		local = next->header.local;
		buddy = next;
		buddy->local = local;
		oslobodi(buddy, stepen);
		osobodjeni_blokovi += pow(2, stepen);
		next = kes->prazan;
	}
	if (!osobodjeni_blokovi)
		kes->error = NE_MOZE_DA_SE_SRINKUJE;
	return osobodjeni_blokovi;
}

void kes_info(Kes* kes)
{
	int index = index_tipskog_kesa(kes);
	if (index == -1)
		return;
	printf("\n*** Kes informacije ***\n");
	printf("Naziv : %s\n", kes->naziv);
	printf("\tVelicina podatka : %d\n", kes->velicina);

	//printf("Size of kes : %d\n",); //veličina celog keša izraženog u broju blokova

	unsigned broj_ploca = 0;
	Slab_block* next;
	next = kes->prazan;
	while (next) {
		broj_ploca++;
		next = next->header.sledeci;
	}
	next = kes->nepun;
	while (next) {
		broj_ploca++;
		next = next->header.sledeci;
	}
	next = kes->pun;
	while (next) {
		broj_ploca++;
		next = next->header.sledeci;
	}

	printf("\tBroj ploca : %d\n", broj_ploca);
	//printf("\tBroj objekata u jednoj ploci : %d\n", );
	//printf("\tPopunjenost : %d\n", );


	//printf("\tLast error: %d\n", kes->error);
	//printf("\tConstructor : %p\n", kes->ctor);
	//printf("\tDestructor : %p\n", kes->dtor);

	printf("\nPuni slabovi : \n");
	slab_info(kes->pun);

	printf("\nNepuni slabovi : \n");
	slab_info(kes->nepun);

	printf("\nPrazni slabovi : \n");
	slab_info(kes->prazan);
	printf("\n******************\n");

}

int kes_error(Kes* kes)
{
	int error;
	int index = index_tipskog_kesa(kes);
	if (index == -1) {
		printf("Ne postoji kes");
		return -1;
	}
	switch (kes->error)
	{
	case 0:
		printf("NO_ERROS");
		return kes->error;
	case 1:
		printf("NE_POSTOJI_KES");
		return kes->error;
	case 2:
		printf("NUSPELA_ALOKACIJA_SLABA");
		return kes->error;
	case 3:
		printf("NUSPELA_ALOKACIJA_SLOTA");
		return kes->error;
	case 4:
		printf("OBJEKAT_ZA_KOJE_JE_ZATEVANO_BRISANJE_SE_NE_NALAZI_U_KESU");
		return kes->error;
	case 5:
		printf("NE_MOZE_DA_SE_SRINKUJE");
		return kes->error;
	default:
		printf("Nedefinisan error");
		return -1;
	}
	return 0;
}
