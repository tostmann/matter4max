import sys
try:
    import chip
    import chip.setup_payload
    from chip.setup_payload.setup_payload import SetupPayload
    payload = SetupPayload()
    payload.setUpPINCode = 86801101
    payload.discriminator = 868
    payload.vendorID = 0xFFF1
    payload.productID = 0x8000
    payload.version = 0
    payload.commissioningFlow = 0 # Standard
    
    # We need the generator:
    from chip.setup_payload import ManualSetupPayloadGenerator, QRCodeSetupPayloadGenerator
    manual_generator = ManualSetupPayloadGenerator.ManualSetupPayloadGenerator(payload)
    print("Manual Code:", manual_generator.payloadDecimalStringRepresentation())
except Exception as e:
    print("Error:", e)
