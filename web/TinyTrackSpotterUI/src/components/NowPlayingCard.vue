<template>
  <q-card>
    <q-card-section>
      <q-item>
        <q-item-section avatar>
          <q-img :src="currentlyPlaying?.item?.album.images[0].url" />
        </q-item-section>
        <q-item-section>
          <q-item-label lines="1" caption>{{ currentlyPlaying?.item?.name }}</q-item-label>
          <q-item-label lines="1" caption>{{ currentlyPlaying?.item?.artists.map(a => a.name).join(', ')
            }}</q-item-label>
        </q-item-section>
      </q-item>
    </q-card-section>
    <q-card-section>
      <q-slider :model-value="progress_ms" :max="currentlyPlaying?.item?.duration_ms" readonly />
    </q-card-section>
    <q-card-actions align="right">
      <!-- <q-btn-group>
      <q-btn flat round icon="pause" v-if="isActive" @click="pause" />
      <q-btn flat round icon="play_arrow" v-else @click="resume" />
    </q-btn-group> -->
    </q-card-actions>

  </q-card>
</template>

<script setup lang="ts">
import { useIntervalFn, useTimeoutPoll } from '@vueuse/core';
import { useObservable, from as fromRef } from '@vueuse/rxjs';
import { storeToRefs } from 'pinia';
import { PlaybackState } from 'src/composables/PlaybackState';
import { useSpotifyClient } from 'src/composables/spotifyClient';
import { useSpotifyStore } from 'src/stores/spotifyStore';
import { ref, watch, computed, toRefs } from 'vue';
import { from, repeat, catchError, of, switchMap, interval, concat, map, take, defaultIfEmpty, timer, mergeMap, defer, tap } from 'rxjs';

const store = useSpotifyStore();
// const currentlyPlaying = ref<PlaybackState | null>(null);
// const progress_ms = ref<number | null>(null);

const currentlyPlaying = useObservable(defer(() => fetchData()).pipe(
  tap(x => console.log('data', x)),
  take(1),
  catchError(() => of(null)),
  defaultIfEmpty(null),
  repeat({
    delay: () => {
      console.log('delay');
      return timer(5000);
    }
  })
  ,
), { initialValue: null })
const intervalT = 100
const progress_ms = useObservable(fromRef(currentlyPlaying).pipe(
  switchMap((x) => x ? concat(of(x.progress_ms), interval(intervalT).pipe(map(y => x.progress_ms + y * intervalT))) : of(null)),
), { initialValue: null })
const progress = computed(() => progress_ms.value && currentlyPlaying.value?.item.duration_ms ? progress_ms.value / currentlyPlaying.value?.item.duration_ms : 0);
// const { pause: pauseIntervall, resume: resumeIntervall, isActive: isActiveIntervall } = useIntervalFn(() => {
//   progress_ms.value = progress_ms.value ? progress_ms.value + 100 : null;
// }, 10000)

async function fetchData () {
  console.log('fetching data');
  try {
    const x = await store.useClient().getCurrentlyPlaying();
    console.log('data', x)
    return x;
  }
  catch (error) {
    console.error(error);
    return null;
  }
  // console.log(currentlyPlaying.value);
  // progress_ms.value = currentlyPlaying.value?.progress_ms ?? null;
  // if(!isActiveIntervall.value) {
  //   resumeIntervall();
  // }
}

// const { isActive: isActivePoll, pause: pausePoll, resume: resumePoll } = useTimeoutPoll(fetchData, 5000)

// const isActive = computed(() => isActiveIntervall.value || isActivePoll.value);

function pause () {
  // pauseIntervall();
  // pausePoll();
}

function resume () {
  // resumePoll();
}
</script>
