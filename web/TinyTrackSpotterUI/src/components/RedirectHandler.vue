<template>
  <div style="display: none;"></div>
</template>
<script setup lang="ts">
import { onMounted } from 'vue';
import { useSpotifyStore } from 'src/stores/spotifyStore';
import { useQuasar } from 'quasar';

const search = window.location.search.substring(1);
const params = new URLSearchParams(search);
const code = params.get('code');
// remove param from url
window.history.pushState('', '', window.location.origin + window.location.pathname)
onMounted(async () => {
  const $q = useQuasar();
  const store = useSpotifyStore();
  if (typeof code === 'string' && store.clientId && store.clientSecret) {
    try {
      await store.useClient().fetchAccessToken(code);
      $q.notify({
        color: 'positive',
        textColor: 'white',
        icon: 'done',
        message: 'redirectHandler.loginSuccess'
      })
    }
    catch (error) {
      console.error(error);
      $q.notify({
        color: 'negative',
        textColor: 'white',
        icon: 'warning',
        message: 'redirectHandler.loginFailed'
      })
    }

  }
  else {
    console.log('no code')
  }
})
</script>
