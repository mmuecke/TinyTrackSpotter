import { useAsyncState } from '@vueuse/core';
import { defineStore } from 'pinia';
import { useEspClient } from 'src/composables/espClient';
import { computed } from 'vue';

export const useEspStore = defineStore('sopitfy', () => {

  const client = useEspClient('http://spotifydisplay.local')
  const { isLoading: isLoadingClientId, state: clientId, isReady: isReadyClientId, execute: executeCientId } = useAsyncState(
    async (args) => {
      return await client.get('clientId')
    },
    {},
    {
      delay: 2000,
      resetOnExecute: false,
    },
  )
  async function setClientId (clientId: string) {
    await client.set('clientId', clientId)
  }
  const { isLoading: isLoadingClientSecret, state: clientSecret, isReady: isReadyClientSecret, execute: executeClientSecret } = useAsyncState(
    async (args) => {
      return await client.get('clientSecret')
    },
    {},
    {
      delay: 2000,
      resetOnExecute: false,
    },
  )
  const { isLoading: isLoadingAccessToken, state: accessToken, isReady: isReadyAccessToken, execute: executeAccessToken } = useAsyncState(
    async (args) => {
      return await client.get('accessToken')
    },
    {},
    {
      delay: 2000,
      resetOnExecute: false,
    },
  )
  const isLoading = computed(() => isLoadingClientId.value || isLoadingClientSecret.value || isLoadingAccessToken.value)
  const isReady = computed(() => isReadyClientId.value && isReadyClientSecret.value && isReadyAccessToken.value)
  async function execute (delay?: number) {
    // await executeCientId(delay)
    // await executeClientSecret(delay)
    // await executeAccessToken(delay)
  }
  return {
    isLoading, clientId, clientSecret, accessToken, isReady, execute, setClientId: (x: string) => setClientId(x)
  }
});
