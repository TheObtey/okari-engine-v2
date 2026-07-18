#include "Formats/J3D/J3DVertexDecoder.h"

#include "IO/BigEndianReader.h"

#include <cmath>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

namespace Okari
{
	namespace
	{
		J3DVertexDecodeResult Failure(const std::string& message)
		{
			J3DVertexDecodeResult result;
			result.Error = message;
			return result;
		}

		bool IsFinite(const glm::vec2& value)
		{
			return
				std::isfinite(value.x) &&
				std::isfinite(value.y);
		}

		bool IsFinite(const glm::vec3& value)
		{
			return
				std::isfinite(value.x) &&
				std::isfinite(value.y) &&
				std::isfinite(value.z);
		}

		float ReadNumericComponent(
			BigEndianReader& reader,
			const J3DVertexFormatDescriptor& format
		)
		{
			if (
				format.ComponentType != 4 &&
				format.FractionalBits > 31
				)
			{
				throw std::runtime_error("VTX1 fixed-point shift exceeds 31 bits");
			}

			const float fixedPointScale =
				std::ldexp(
					1.0f,
					-static_cast<int>(
						format.FractionalBits
						)
				);

			switch (format.ComponentType)
			{
			case 0: // U8
				return
					static_cast<float>(
						reader.ReadU8()
						) *
					fixedPointScale;

			case 1: // S8
				return
					static_cast<float>(
						reader.ReadS8()
						) *
					fixedPointScale;

			case 2: // U16
				return
					static_cast<float>(
						reader.ReadU16()
						) *
					fixedPointScale;

			case 3: // S16
				return
					static_cast<float>(
						reader.ReadS16()
						) *
					fixedPointScale;

			case 4: // F32
				return reader.ReadF32();

			default:
				throw std::runtime_error(
					"Unsupported VTX1 numeric component type"
				);
			}
		}

		std::uint8_t Expand4To8(
			std::uint8_t value
		)
		{
			return static_cast<std::uint8_t>(
				(value << 4) |
				value
				);
		}

		std::uint8_t Expand5To8(
			std::uint8_t value
		)
		{
			return static_cast<std::uint8_t>(
				(value << 3) |
				(value >> 2)
				);
		}

		std::uint8_t Expand6To8(
			std::uint8_t value
		)
		{
			return static_cast<std::uint8_t>(
				(value << 2) |
				(value >> 4)
				);
		}

		J3DColorRGBA8 ReadColor(
			BigEndianReader& reader,
			const J3DVertexFormatDescriptor& format
		)
		{
			J3DColorRGBA8 color;

			switch (format.ComponentType)
			{
			case 0: // RGB565
			{
				const std::uint16_t packed =
					reader.ReadU16();

				color.R = Expand5To8(
					static_cast<std::uint8_t>(
						(packed >> 11) & 0x1F
						)
				);

				color.G = Expand6To8(
					static_cast<std::uint8_t>(
						(packed >> 5) & 0x3F
						)
				);

				color.B = Expand5To8(
					static_cast<std::uint8_t>(
						packed & 0x1F
						)
				);

				color.A = 255;
				break;
			}

			case 1: // RGB8
				color.R = reader.ReadU8();
				color.G = reader.ReadU8();
				color.B = reader.ReadU8();
				color.A = 255;
				break;

			case 2: // RGBX8
				color.R = reader.ReadU8();
				color.G = reader.ReadU8();
				color.B = reader.ReadU8();

				// Composante X inutilisée.
				reader.Skip(1);

				color.A = 255;
				break;

			case 3: // RGBA4
			{
				const std::uint16_t packed =
					reader.ReadU16();

				color.R = Expand4To8(
					static_cast<std::uint8_t>(
						(packed >> 12) & 0x0F
						)
				);

				color.G = Expand4To8(
					static_cast<std::uint8_t>(
						(packed >> 8) & 0x0F
						)
				);

				color.B = Expand4To8(
					static_cast<std::uint8_t>(
						(packed >> 4) & 0x0F
						)
				);

				color.A = Expand4To8(
					static_cast<std::uint8_t>(
						packed & 0x0F
						)
				);

				break;
			}

			case 4: // RGBA6
			{
				const std::uint32_t packed =
					(
						static_cast<std::uint32_t>(
							reader.ReadU8()
							) << 16
						) |
					(
						static_cast<std::uint32_t>(
							reader.ReadU8()
							) << 8
						) |
					static_cast<std::uint32_t>(
						reader.ReadU8()
						);

				color.R = Expand6To8(
					static_cast<std::uint8_t>(
						(packed >> 18) & 0x3F
						)
				);

				color.G = Expand6To8(
					static_cast<std::uint8_t>(
						(packed >> 12) & 0x3F
						)
				);

				color.B = Expand6To8(
					static_cast<std::uint8_t>(
						(packed >> 6) & 0x3F
						)
				);

				color.A = Expand6To8(
					static_cast<std::uint8_t>(
						packed & 0x3F
						)
				);

				break;
			}

			case 5: // RGBA8
				color.R = reader.ReadU8();
				color.G = reader.ReadU8();
				color.B = reader.ReadU8();
				color.A = reader.ReadU8();
				break;

			default:
				throw std::runtime_error("Unsupported VTX1 color component type");
			}

			return color;
		}

		std::size_t GetColorChannel(
			J3DVertexAttribute attribute
		)
		{
			switch (attribute)
			{
			case J3DVertexAttribute::Color0:
				return 0;

			case J3DVertexAttribute::Color1:
				return 1;

			default:
				throw std::runtime_error("VTX1 attribute is not a color channel");
			}
		}

		std::size_t GetTexCoordChannel(
			J3DVertexAttribute attribute
		)
		{
			const std::uint32_t rawAttribute =
				static_cast<std::uint32_t>(
					attribute
					);

			const std::uint32_t firstTexCoord =
				static_cast<std::uint32_t>(
					J3DVertexAttribute::TexCoord0
					);

			const std::uint32_t lastTexCoord =
				static_cast<std::uint32_t>(
					J3DVertexAttribute::TexCoord7
					);

			if (
				rawAttribute < firstTexCoord ||
				rawAttribute > lastTexCoord
				)
			{
				throw std::runtime_error("VTX1 attribute is not a texture coordinate");
			}

			return static_cast<std::size_t>(rawAttribute - firstTexCoord);
		}

		bool IsTexCoordAttribute(
			J3DVertexAttribute attribute
		)
		{
			const std::uint32_t rawAttribute =
				static_cast<std::uint32_t>(
					attribute
					);

			return
				rawAttribute >=
				static_cast<std::uint32_t>(
					J3DVertexAttribute::TexCoord0
					) &&
				rawAttribute <=
				static_cast<std::uint32_t>(
					J3DVertexAttribute::TexCoord7
					);
		}
	}

	J3DVertexDecodeResult J3DVertexDecoder::Decode(
		const J3DVTX1Data& vtx1,
		const J3DVertexDecodeRequest& request
	)
	{
		try
		{
			J3DDecodedVertexData decoded;

			for (
				const J3DVertexArrayData& array :
				vtx1.Arrays
				)
			{
				const J3DVertexFormatDescriptor* format = vtx1.FindFormat(array.Attribute);

				if (format == nullptr)
					return Failure("Cannot decode VTX1 array without its format");
				
				std::size_t elementCount = 0;

				switch (array.Attribute)
				{
				case J3DVertexAttribute::Position:
					elementCount = request.PositionCount;
					break;

				case J3DVertexAttribute::Normal:
				case J3DVertexAttribute::NBT:
					elementCount =
						format->ComponentCount == 0
						? request.NormalCount
						: request.NBTFrameCount;

					break;

				case J3DVertexAttribute::Color0:
					elementCount = request.ColorCounts[0];
					break;

				case J3DVertexAttribute::Color1:
					elementCount = request.ColorCounts[1];
					break;

				default:
					if (IsTexCoordAttribute(array.Attribute))
					{
						elementCount =
							request.TexCoordCounts[
								GetTexCoordChannel(array.Attribute)
							];
					}
					else
					{
						return Failure(
							"Unsupported VTX1 attribute during count resolution"
						);
					}

					break;
				}

				if (elementCount == 0)
					continue;

				if (elementCount > array.ElementCapacity)
				{
					return Failure(
						"Requested VTX1 element count exceeds capacity for " +
						std::string(ToString(array.Attribute))
					);
				}

				const std::uint64_t requiredByteCount =
					static_cast<std::uint64_t>(
						elementCount
						) *
					array.ElementStride;

				if (requiredByteCount > array.RawData.size())
					return Failure("VTX1 decoded element range exceeds raw data");
				
				BigEndianReader reader(array.RawData);

				switch (array.Attribute)
				{
				case J3DVertexAttribute::Position:
				{
					decoded.Positions.reserve(
						elementCount
					);

					for (
						std::size_t index = 0;
						index < elementCount;
						++index
						)
					{
						glm::vec3 position(0.0f);

						position.x =
							ReadNumericComponent(
								reader,
								*format
							);

						position.y =
							ReadNumericComponent(
								reader,
								*format
							);

						if (format->ComponentCount == 1)
						{
							position.z =
								ReadNumericComponent(
									reader,
									*format
								);
						}

						if (!IsFinite(position))
							return Failure("VTX1 position contains a non-finite value");
						
						decoded.Positions.push_back(position);
					}

					break;
				}

				case J3DVertexAttribute::Normal:
				case J3DVertexAttribute::NBT:
				{
					if (format->ComponentCount == 0)
					{
						decoded.Normals.reserve(
							decoded.Normals.size() +
							elementCount
						);

						for (
							std::size_t index = 0;
							index < elementCount;
							++index
							)
						{
							glm::vec3 normal;

							normal.x =
								ReadNumericComponent(
									reader,
									*format
								);

							normal.y =
								ReadNumericComponent(
									reader,
									*format
								);

							normal.z =
								ReadNumericComponent(
									reader,
									*format
								);

							if (!IsFinite(normal))
								return Failure("VTX1 normal contains a non-finite value");
							
							decoded.Normals.push_back(normal);
						}
					}
					else
					{
						decoded.NBTFrames.reserve(
							decoded.NBTFrames.size() +
							elementCount
						);

						for (
							std::size_t index = 0;
							index < elementCount;
							++index
							)
						{
							J3DNBTFrame frame;

							frame.Normal.x = ReadNumericComponent(reader, *format);
							frame.Normal.y = ReadNumericComponent(reader, *format);
							frame.Normal.z = ReadNumericComponent(reader, *format);

							frame.Binormal.x = ReadNumericComponent(reader, *format);
							frame.Binormal.y = ReadNumericComponent(reader, *format);
							frame.Binormal.z = ReadNumericComponent(reader, *format);

							frame.Tangent.x = ReadNumericComponent(reader, *format);
							frame.Tangent.y = ReadNumericComponent(reader, *format);
							frame.Tangent.z = ReadNumericComponent(reader, *format);

							if (
								!IsFinite(frame.Normal) ||
								!IsFinite(frame.Binormal) ||
								!IsFinite(frame.Tangent)
								)
							{
								return Failure("VTX1 NBT frame contains a non-finite value");
							}

							decoded.NBTFrames.push_back(frame);
						}
					}

					break;
				}

				case J3DVertexAttribute::Color0:
				case J3DVertexAttribute::Color1:
				{
					const std::size_t channel =
						GetColorChannel(
							array.Attribute
						);

					std::vector<J3DColorRGBA8>& colors = decoded.Colors[channel];

					colors.reserve(elementCount);

					for (
						std::size_t index = 0;
						index < elementCount;
						++index
						)
					{
						colors.push_back(
							ReadColor(
								reader,
								*format
							)
						);
					}

					break;
				}

				default:
					if (IsTexCoordAttribute(array.Attribute))
					{
						const std::size_t channel =
							GetTexCoordChannel(
								array.Attribute
							);

						std::vector<glm::vec2>& texCoords = decoded.TexCoords[channel];

						texCoords.reserve(elementCount);

						for (
							std::size_t index = 0;
							index < elementCount;
							++index
							)
						{
							glm::vec2 texCoord(0.0f);

							texCoord.x =
								ReadNumericComponent(
									reader,
									*format
								);

							if (format->ComponentCount == 1)
							{
								texCoord.y =
									ReadNumericComponent(
										reader,
										*format
									);
							}

							if (!IsFinite(texCoord))
								return Failure("VTX1 texture coordinate contains a non-finite value");
							
							texCoords.push_back(texCoord);
						}
					}
					else
					{
						return Failure("Unsupported VTX1 attribute during decoding");
					}

					break;
				}

				if (
					reader.Tell() !=
					static_cast<std::size_t>(
						requiredByteCount
						)
					)
				{
					return Failure(
						"VTX1 decoder consumed an unexpected byte count for " +
						std::string(
							ToString(array.Attribute)
						)
					);
				}
			}

			if (decoded.Positions.size() != request.PositionCount)
				return Failure("VTX1 decoded position count does not match INF1");

			if (decoded.Normals.size() != request.NormalCount)
			{
				return Failure(
					"VTX1 decoded normal count does not match the request"
				);
			}

			if (decoded.NBTFrames.size() != request.NBTFrameCount)
			{
				return Failure(
					"VTX1 decoded NBT count does not match the request"
				);
			}

			for (std::size_t channel = 0; channel < 2; ++channel)
			{
				if (
					decoded.Colors[channel].size() !=
					request.ColorCounts[channel]
					)
				{
					return Failure(
						"VTX1 decoded color count does not match the request"
					);
				}
			}

			for (std::size_t channel = 0; channel < 8; ++channel)
			{
				if (
					decoded.TexCoords[channel].size() !=
					request.TexCoordCounts[channel]
					)
				{
					return Failure(
						"VTX1 decoded texture coordinate count does not match the request"
					);
				}
			}
			
			J3DVertexDecodeResult result;
			result.Data = std::move(decoded);

			return result;
		}
		catch (const std::exception& exception)
		{
			return Failure(
				std::string(
					"Unable to decode VTX1 vertex data: "
				) +
				exception.what()
			);
		}
	}
}