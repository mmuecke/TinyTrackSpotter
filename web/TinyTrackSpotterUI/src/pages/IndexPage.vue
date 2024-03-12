<template>
  <q-page class="row items-center justify-evenly">
    <div class="column">
      <credentials-form />
      <PlaybackState />
      <SettingsCard />
      <redirect-handler />
    </div>
  </q-page>
</template>

<script setup lang="ts">
import CredentialsForm from 'src/components/CredentialsForm.vue';
import RedirectHandler from 'src/components/RedirectHandler.vue';
import PlaybackState from 'src/components/NowPlayingCard.vue';
import SettingsCard from 'src/components/SettingsCard.vue';
import { useRouter } from 'vue-router';
const $router = useRouter();
console.log($router.currentRoute.value.query)
</script>
